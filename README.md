# Gill — From Minus to Peak

Personal portfolio site for Ragil Vahlevi ("Gill"), a Computer, Network &
Telecommunication Engineering (TJKT) student. Built with Astro,
TypeScript, and Tailwind CSS.

This archive corresponds to **Prompt 01**: project foundation, design
system, component architecture, and static page structure. No backend,
no AI assistant, and no real project/achievement data have been added
yet — see `PROJECT_CONTEXT.md` for full details.

## Requirements

- Node.js 18.17+ (or 20+) recommended
- npm

## Getting started

This project was built in a sandbox without network access, so
dependencies have **not** been installed and the build has **not**
been verified yet. Before doing anything else locally:

```bash
npm install
npm run dev       # start the local dev server
npm run build      # type-check (astro check) + production build
npm run preview    # preview the production build locally
```

If `npm install` or `npm run build` surface errors, they have not been
seen or fixed yet — this is the first real compile/build this project
will go through.

## Project structure

```
src/
├── components/
│   ├── layout/       # Container, Footer, SectionShell
│   ├── navigation/   # Nav (desktop + mobile), ProgressRail
│   ├── hero/         # Hero, HeroTerminal
│   ├── sections/     # About, Journey, Skills, Projects, Achievements, Contact
│   ├── cards/         # ProjectCard, AchievementCard, JourneyMilestoneCard
│   ├── terminal/     # TerminalWindow, TerminalLine, TypedText
│   └── ui/            # Button, Badge, SectionHeader, Card, StatusIndicator, TechTag, EmptyState
├── layouts/           # BaseLayout (SEO scaffolding + global chrome)
├── pages/             # index.astro, ai.astro (placeholder route)
├── data/              # Local TypeScript data (profile, journey, skills, projects, achievements, education, nav)
├── types/             # Shared TypeScript interfaces for the data layer
└── styles/            # global.css — design tokens (CSS variables) + Tailwind layers

public/
├── favicon.svg
└── images/
    ├── avatar-placeholder.svg
    └── og-placeholder.svg
```

## Content that still needs to be filled in

Several data files were deliberately left as placeholders rather than
filled with invented content — see the `TODO` comments in:

- `src/data/profile.ts` — real social links, location
- `src/data/journey.ts` — real years for each milestone
- `src/data/education.ts` — real institution names
- `src/data/skills.ts`, `src/data/projects.ts`, `src/data/achievements.ts` — currently empty; populate with real entries
- `src/layouts/BaseLayout.astro` — replace the placeholder OG image and favicon before launch
- `astro.config.mjs` — replace the placeholder `site` URL with the real production domain

## Roadmap (not yet implemented)

- Laravel REST API backend
- Filament admin panel
- "Ask Gill AI" assistant (placeholder page only, at `/ai`)

Do not assume any of the above exists yet — this archive is the static
frontend foundation only.
