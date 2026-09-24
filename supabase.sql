create table public.messages (
  id              bigint generated always as identity primary key,
  created_at      timestamptz not null default now(),
  texte_envoye    text not null,
  texte_recu      text,
  bit_ms          int,
  octets_envoyes  int,
  octets_recus    int,
  bits_faux       int,
  erreurs_trame   int,
  contraste       int
);

alter table public.messages enable row level security;

create policy "lecture publique" on public.messages
  for select to anon using (true);

create policy "ajout public" on public.messages
  for insert to anon with check (true);
