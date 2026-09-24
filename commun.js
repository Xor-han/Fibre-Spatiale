const $ = id => document.getElementById(id);

const cfg = window.SUPABASE_CONFIG || null;
const sb = cfg && cfg.url && cfg.key ? supabase.createClient(cfg.url, cfg.key) : null;

const COL = {
  table:   'messages',
  date:    'date',
  envoye:  'message_envoye',
  bitEnv:  'message_bit_envoye',
  bitRec:  'message_bit_recu',
  retrans: 'message_retranscrit',
  statut:  'statut',
  bitMs:     'bit_ms',
  bitsFaux:  'bits_faux',
  contraste: 'contraste',
};
const STATUT = { encours: 'EN_COURS', ok: 'OK', erreur: 'ERREUR' };
let mesuresDispo = false;

const esc = s => String(s ?? '').replace(/[&<>"]/g, c =>
  ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const bits8 = b => b.toString(2).padStart(8, '0');
const suiteBits = arr => arr.map(bits8).join(' ');
const pause = ms => new Promise(r => setTimeout(r, ms));

async function sonderMesures() {
  if (!sb) return false;
  const { error } = await sb.from(COL.table)
    .select([COL.bitMs, COL.bitsFaux, COL.contraste].join(',')).limit(1);
  mesuresDispo = !error;
  if (!mesuresDispo) console.warn('Colonnes de mesure absentes : exécute supabase-v2.sql.');
  return mesuresDispo;
}

const carte = { port: null, writer: null, attentes: [], surLigne: null };

async function connecterCarte(surLigne, surEtat) {
  carte.surLigne = surLigne;
  try {
    carte.port = await navigator.serial.requestPort();
    await carte.port.open({ baudRate: 115200 });
    carte.writer = carte.port.writable.getWriter();
    surEtat('Connectée', true);
    lireEnBoucle(surEtat);
    return true;
  } catch (e) {
    surEtat('Erreur : ' + e.message, false);
    return false;
  }
}

async function lireEnBoucle(surEtat) {
  const reader = carte.port.readable.pipeThrough(new TextDecoderStream()).getReader();
  let buf = '';
  try {
    while (true) {
      const { value, done } = await reader.read();
      if (done) break;
      buf += value;
      let i;
      while ((i = buf.indexOf('\n')) >= 0) {
        const l = buf.slice(0, i).trim();
        buf = buf.slice(i + 1);
        const att = carte.attentes.find(a => l.startsWith(a.prefixe));
        if (att) {
          carte.attentes = carte.attentes.filter(a => a !== att);
          att.resolve(l);
        }
        carte.surLigne(l);
      }
    }
  } catch (e) {
    carte.writer = null;
    surEtat('Déconnectée : ' + e.message, false);
  }
}

async function versCarte(ligne) {
  if (!carte.writer) return;
  await carte.writer.write(new TextEncoder().encode(ligne + '\n'));
}

function attendreCarte(prefixe, ms = 4000) {
  return new Promise((resolve, reject) => {
    const t = setTimeout(() => {
      carte.attentes = carte.attentes.filter(a => a !== entree);
      reject(new Error('pas de réponse de la carte à ' + prefixe));
    }, ms);
    const entree = { prefixe, resolve: l => { clearTimeout(t); resolve(l); } };
    carte.attentes.push(entree);
  });
}

async function demanderCarte(commande, prefixe, ms = 4000) {
  const attendu = attendreCarte(prefixe, ms);
  await versCarte(commande);
  return attendu;
}

const CANAL = 'lien-optique';
let canal = null;
const ecoutes = [];
const attentesCanal = [];

async function ouvrirCanal(onEtat) {
  if (!sb) { onEtat('Supabase non configuré', false); return false; }
  canal = sb.channel(CANAL, { config: { broadcast: { self: false } } });
  canal.on('broadcast', { event: 'msg' }, ({ payload }) => {
    const att = attentesCanal.find(a => a.types.includes(payload.type));
    if (att) {
      attentesCanal.splice(attentesCanal.indexOf(att), 1);
      att.resolve(payload);
    }
    ecoutes.forEach(f => f(payload));
  });
  await new Promise(resolve => {
    canal.subscribe(st => {
      if (st === 'SUBSCRIBED') { onEtat('ouvert', true); resolve(); }
      else if (st === 'CHANNEL_ERROR' || st === 'TIMED_OUT') {
        onEtat('indisponible (' + st + ')', false); resolve();
      }
    });
  });
  return true;
}

const surCanal = f => ecoutes.push(f);
async function versCanal(payload) {
  if (!canal) return;
  await canal.send({ type: 'broadcast', event: 'msg', payload });
}

function attendreCanal(types, ms = 6000) {
  const liste = Array.isArray(types) ? types : [types];
  return new Promise((resolve, reject) => {
    const t = setTimeout(() => {
      const i = attentesCanal.indexOf(entree);
      if (i >= 0) attentesCanal.splice(i, 1);
      reject(new Error('pas de réponse de l\'autre poste (' + liste.join(' ou ') + ')'));
    }, ms);
    const entree = { types: liste, resolve: p => { clearTimeout(t); resolve(p); } };
    attentesCanal.push(entree);
  });
}

async function demanderCanal(envoi, typeReponse, ms = 6000) {
  const attendu = attendreCanal(typeReponse, ms);
  await versCanal(envoi);
  return attendu;
}
