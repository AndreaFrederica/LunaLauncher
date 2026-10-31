import { configure } from 'quasar/wrappers';

export default configure(() => ({
  boot: ['pinia', 'i18n'],
  css: ['app.scss'],
  extras: ['material-icons', 'roboto-font'],
  build: {
    vueRouterMode: 'hash',
    extendViteConf(viteConf) {
      viteConf.build ??= {};
      viteConf.build.chunkSizeWarningLimit = 2000;
    },
  },
  devServer: { open: false },
  framework: { plugins: ['Notify'], config: { brand: { primary: '#6366f1', secondary: '#94a3b8', positive: '#34d399' } } },
  animations: [],
  sourceFiles: { rootComponent: 'src/NeoRoot.vue' }
}));
