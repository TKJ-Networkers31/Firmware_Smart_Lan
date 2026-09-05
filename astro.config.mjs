import { defineConfig } from "astro/config";
import tailwind from "@astrojs/tailwind";

// https://astro.build/config
export default defineConfig({
  site: "https://gill.dev", // TODO: replace with the production domain
  integrations: [
    tailwind({
      applyBaseStyles: false,
    }),
  ],
  output: "static",
  compressHTML: true,
});
