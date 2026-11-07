# RESTMORE / Visual identity

## North star
RESTMORE should feel like a precise, next-generation instrument for viewing complicated API territory: **calm typography, strong contrast, technical honesty, and alive-but-controlled motion**.

The visuals do not compromise comprehension. Status labels must describe actual provenance, not decorative confidence.

## Identity
- **Wordmark:** `R/M`, angular and minimal. Full name `RESTMORE`, uppercase.
- **Hero language:** “MAP THE UNSEEN.”
- **Core colors:** canvas `#07101b`, secondary canvas `#0e1b2b`, luminous mint `#79f9dc`, primary text `#eef9f7`, secondary text `#96acbf`, fine dividers `#294057`.
- **Typesetting:** display sans with tight tracking; monospace for IDs, paths, version text, methods and compact statuses.
- **Spacing:** multiples of 8 px, with generous white space outside data-dense panels.

## Status and methods
Methods are both **labeled** and color-distinguished. Never use color as the sole indicator:
- GET: mint `#79f9dc`
- POST: warm gold `#e9c078`
- PATCH: violet `#a49bff`
- DELETE: warm coral `#ff9387`

On topology: circular nodes indicate a method, connecting lines show a documented relation, and the origin is the central hub. Display a legend and evidence reference in full production mapping screens. Use dashed lines only for explicitly marked inferred relationships.

## Interaction
- Domain input is the first obvious action, with scope/permission information adjacent to the action.
- **Preview** and **Run** cannot be confused. Network activity requires user authorization.
- Make progress accessible in text. Provide an explanation for every failure state.
- Use keyboard-accessible controls, visible focus indication, appropriate semantic labels, and reduced-motion support.
- Avoid rendering hundreds of node labels simultaneously: summarize clusters and permit inspection.

## Visual illustration specification
1. **System orbit**: concentric map rings around an illuminated R/M monogram. Decorative, not a live telemetry claim.
2. **Topology view**: central API origin with color-coded operation nodes, label collision management and clear source provenance.
3. **Pipeline diagram**: authorization → documentation → evidence hash → parser → catalog → exports.
4. **Source lineage**: operation linked back to exact source URL and document SHA-256.
5. **Platform matrix**: clearly separate implementation, compile testing, device runtime testing, and signed distribution.

GitHub Pages implements illustration 1 and an interactive **synthetic** example of illustration 2. Markdown diagrams implement 3–5.

## Native GUI alignment checklist
- Use the same numeric color tokens as the web.
- Headings and cards have consistent padding and readable contrast.
- Show scan state, navigation state, host and request limits.
- Operation map labels should clip or pan safely rather than overlap arbitrary nodes.
- Never obstruct required controls on small displays.
- Verify screenshots at common Windows/Linux/macOS resolutions and Android portrait orientation.
