# Spyro the Dragon

`SCUS_942.28` (USA), authenticated by `executable.json`. `spyro1::Runtime` derives from the shared
`spyro::Runtime`, binds the stock libraries at this image's addresses and installs its native leaves.
Every remaining guest instruction runs from the authenticated image through Lightrec.

It presents on the Record path: `spyro1::FrameDriver` runs the retail update, then GamestateDraw
`0x8001ED5C` through its own display tail (`spyro1::FrameDraw`), and presents the frame once. libgte's
projection leaves (`0x80062618`, `0x80062638`) are stock bindings and the canvas widens through
`spyro::Runtime::guestWidescreenProjection`. Its 60 fps producers (psxport F-FPS60,
`docs/features.md`) are the fullscreen tint and the cinematic letterbox
(`titles/spyro1/render/screen_overlays.*`), slotted by the LinkOTPrimitives override
(`titles/spyro1/render/ordering_table.*`); culls, HUD anchoring and the terrain, actor and
sky producers are issue 0186.
