# Renderer Rewrite

Current renderer performance is hard to optimize and isn't even great at what it does. Rewriting the entire renderer using a pbr study and methods used by other engines.
This will be a multi stage plan so we can verify at various point in between and sort of monitor progress.

## Phases

- Phase 1: Reset renderer to a simple hull
- Phase 2: Add opaque pbr materials only using forward+ rendering
- Phase 3: Shadows & MSAA
- Phase 4: Transparent materials and other related
- Phase 5: Reflections and global illumination
