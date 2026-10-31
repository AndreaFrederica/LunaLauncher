import { defineBoot } from '#q-app/wrappers';
import { createPinia } from 'pinia';

export default defineBoot(({ app }) => { app.use(createPinia()); });
