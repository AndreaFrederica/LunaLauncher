import { configure } from 'quasar/wrappers';

export default configure(() => ({
  boot: [],
  css: ['app.scss'],
  extras: ['material-icons', 'roboto-font'],
  build: { vueRouterMode: 'hash' },
  devServer: { open: false },
  framework: { plugins: [], config: { brand: { primary: '#6366f1', secondary: '#94a3b8', positive: '#34d399' } } },
  animations: [],
  sourceFiles: { rootComponent: 'src/App.vue' }
}));
