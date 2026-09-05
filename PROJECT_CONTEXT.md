# Project Context — Gill Portfolio

This file tracks what has actually been built, what decisions were
made and why, and what constraints still apply — so future work (by
Claude or anyone else) doesn't accidentally re-litigate or violate
earlier decisions.

## Status: Prompt 01 — complete

Prompt 01 scope was: establish the Astro project, design system,
reusable components, page architecture, local data structures, initial
visual foundation, hero structure, and responsive/performant baseline.

That scope is complete as of this archive. **Prompt 02 has not started.**

## Tech stack (fixed — do not change without explicit request)

- Astro (static output), TypeScript (strict), Tailwind CSS
- No React/Vue/Svelte — none is currently needed
- No backend yet. Data lives in `src/data/*.ts`, typed via
  `src/types/index.ts`, structured so it can later be swapped for a
  Laravel REST API response without touching component code

## Design concept

"From minus to peak" — Gill's real story: an IPA/science background,
not getting into the originally intended school, landing in TJKT
without initial interest, and gradually finding direction through
learning, reflection, leadership, and competitions. The site must never
imply the peak has been reached — always "still climbing."

Visual direction chosen for Prompt 01 (see `src/styles/global.css` for
the token values):

- Dark, near-black base (`--color-void`, slightly cool-tinted rather
  than pure black) with a desaturated **signal teal** accent
  (`--color-signal`) — deliberately not the generic "acid-green/vermilion
  on near-black" AI-portfolio default, and not a warm terracotta/cream
  palette either. The accent reads as instrumentation (a status light,
  a trace on a scope) rather than decoration.
- Typography: Space Grotesk (display/headings), Inter (body), JetBrains
  Mono (terminal/system/metadata labels only — not the whole site)
- The mountain/elevation metaphor is rendered as instrumentation (a
  vertical scroll-progress "elevation rail," faint contour-line
  backdrops, a plotted ascent line) — never literal hiking/outdoor
  imagery
- Section numbering (01–06) is used because the content genuinely is a
  sequence (About → Journey → Skills → Projects → Achievements →
  Contact)

## Explicit constraints carried over from the original brief

These still apply to all future prompts unless the person says
otherwise:

- Do not invent fake projects, achievements, testimonials, GitHub
  activity, or statistics. `src/data/projects.ts` and
  `src/data/achievements.ts` are intentionally empty; `src/data/skills.ts`
  has category shells with empty skill lists. Sections render an
  honest "awaiting entries" empty state rather than filler content.
- Do not claim Gill is a professional or expert.
- Do not implement the Laravel backend, Filament admin, or the "Ask
  Gill AI" assistant yet. `src/pages/ai.astro` is a placeholder route
  only (so the nav link isn't a 404), explicitly not the real feature.
- Keep the site lightweight: minimal JS (only two small vanilla
  `<script>` blocks — the mobile menu toggle and the scroll-progress
  rail — no framework, no animation library), Astro static rendering,
  `prefers-reduced-motion` respected.

## Known TODOs left in the code (search for `TODO` comments)

- Real years in `src/data/journey.ts`
- Real institution names in `src/data/education.ts`
- Real social links / location in `src/data/profile.ts`
- Real favicon/avatar photo/OG image (current SVGs in `public/` are
  intentional placeholders, styled to match the design system so
  they're not visually jarring, but are not final assets)
- Real production domain in the `site` field of `astro.config.mjs`
- `profile.avatarPlaceholder` exists as a data field and the referenced
  SVG asset exists, but no component currently renders it (the Hero
  has no `<img>` yet) — flagged, not fixed, since adding it would be a
  UI change beyond what was asked for when this was last reviewed

## Verification limitations (as of this archive)

This project was built and reviewed in a sandboxed environment with
**no network access**. That means:

- `npm install` has never been run against this exact `package.json`
- `astro check` / `tsc` have never actually been executed
- `npm run build` / `npm run dev` have never actually been executed

All correctness checks so far have been manual: reading source files,
cross-referencing imports against actual file paths, and cross-referencing
data files against their TypeScript interfaces. This is not a substitute
for actually installing and building the project. Do this first, locally,
before adding new features.
