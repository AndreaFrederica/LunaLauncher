// SPDX-License-Identifier: MPL-2.0
import { createApp } from 'vue';
import { Quasar } from 'quasar';
import { createPinia } from 'pinia';
import App from './App.vue';
import { i18n } from './i18n';
import 'quasar/src/css/index.sass';
// Shared surface, scrollbar, drag and card tokens copied from XMCL's MIT UI.
import './xmcl-port/assets/common.css';

createApp(App).use(Quasar, {}).use(createPinia()).use(i18n).mount('#q-app');
