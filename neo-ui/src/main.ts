import { createApp } from 'vue';
import { Quasar } from 'quasar';
import { createPinia } from 'pinia';
import App from './App.vue';
import 'quasar/src/css/index.sass';
// Shared surface, scrollbar, drag and card tokens copied from XMCL's MIT UI.
import './xmcl-port/assets/common.css';

createApp(App).use(Quasar, {}).use(createPinia()).mount('#q-app');
