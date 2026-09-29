-- Training metrics tables for actprove-drone (run in Supabase SQL Editor)
create extension if not exists "pgcrypto";

create table if not exists public.training_runs (
  id text primary key,
  started_at timestamptz,
  model text,
  epochs int,
  git_sha text,
  notes text,
  created_at timestamptz not null default now()
);

create table if not exists public.training_metrics (
  id uuid primary key default gen_random_uuid(),
  run_id text not null references public.training_runs (id) on delete cascade,
  split text not null,
  map50 double precision,
  map50_95 double precision,
  per_class jsonb,
  fp_on_negatives double precision,
  created_at timestamptz not null default now()
);

create table if not exists public.training_artifacts (
  id uuid primary key default gen_random_uuid(),
  run_id text not null references public.training_runs (id) on delete cascade,
  kind text not null,
  storage_path text not null,
  created_at timestamptz not null default now()
);

alter table public.training_runs enable row level security;
alter table public.training_metrics enable row level security;
alter table public.training_artifacts enable row level security;

-- Service role bypasses RLS; allow authenticated read for dashboard later
drop policy if exists training_runs_select_authenticated on public.training_runs;
create policy training_runs_select_authenticated on public.training_runs
  for select to authenticated using (true);

drop policy if exists training_metrics_select_authenticated on public.training_metrics;
create policy training_metrics_select_authenticated on public.training_metrics
  for select to authenticated using (true);

drop policy if exists training_artifacts_select_authenticated on public.training_artifacts;
create policy training_artifacts_select_authenticated on public.training_artifacts
  for select to authenticated using (true);
