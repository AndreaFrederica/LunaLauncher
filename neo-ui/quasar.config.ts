import { configure } from 'quasar/wrappers';

export default configure(() => ({
  boot: [],
  css: ['app.scss'],
  extras: ['material-icons', 'roboto-font'],
  build: { vueRouterMode: 'hash' },
  devServer: { open: false },
  framework: { plugins: [] },
  animations: [],
  sourceFiles: { rootComponent: 'src/App.vue' }
}));
