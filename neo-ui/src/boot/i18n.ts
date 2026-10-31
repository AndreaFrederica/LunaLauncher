import { defineBoot } from '#q-app/wrappers';
import { i18n } from '../i18n';

export default defineBoot(({ app }) => { app.use(i18n); });
