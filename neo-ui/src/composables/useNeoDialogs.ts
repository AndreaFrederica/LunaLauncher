import { ref } from 'vue';

export function useNeoDialogs() {
  const dialog = ref(false);
  const prompt = ref('');
  const answer = ref<any>('');
  const choices = ref<any[] | null>(null);
  const secret = ref(false);
  const confirmation = ref(false);
  let resolveDialog: ((value:any) => void) | undefined;

  function ask(title:string, value:any = '', confirm = false, options:any[] | null = null, password = false):Promise<any> {
    prompt.value = title;
    answer.value = options?.length ? options[0].value : value;
    confirmation.value = confirm;
    choices.value = options;
    secret.value = password;
    dialog.value = true;
    return new Promise(resolve => { resolveDialog = resolve; });
  }

  function finishDialog(ok:boolean) {
    dialog.value = false;
    resolveDialog?.(ok ? (confirmation.value ? true : answer.value) : null);
    resolveDialog = undefined;
  }

  function message(error:any):string {
    if (error instanceof Error) return error.message;
    if (typeof error === 'string') return error;
    if (error?.message) return message(error.message);
    if (error?.error) return message(error.error);
    try { return JSON.stringify(error); } catch { return String(error); }
  }

  return { dialog, prompt, answer, choices, secret, confirmation, ask, finishDialog, message };
}
