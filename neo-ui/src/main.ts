import { createApp } from 'vue';
import { Quasar } from 'quasar';
import { createPinia } from 'pinia';
import App from './App.vue';
import 'quasar/src/css/index.sass';

createApp(App).use(Quasar, {}).use(createPinia()).mount('#q-app');
