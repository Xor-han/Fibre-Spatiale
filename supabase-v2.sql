create policy "maj publique" on public.messages
  for update to anon using (true) with check (true);

alter table public.messages
  add constraint statut_valide
  check (statut in ('EN_COURS', 'OK', 'ERREUR'));

alter table public.messages
  add column if not exists bit_ms    int,
  add column if not exists bits_faux int,
  add column if not exists contraste int;

