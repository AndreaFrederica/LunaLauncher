// The launcher sends binary confirmations as a select request with these two choices.
export function isContinueConfirmation(choices?: readonly unknown[] | null): boolean {
  return choices?.length === 2 && choices[0] === 'Cancel' && choices[1] === 'Continue';
}
