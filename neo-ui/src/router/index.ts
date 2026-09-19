import { createRouter, createWebHashHistory, RouteRecordRaw } from 'vue-router';
import IndexPage from '../pages/IndexPage.vue';

const routes: RouteRecordRaw[] = [{ path: '/', component: IndexPage }];
export default createRouter({ history: createWebHashHistory(), routes });
