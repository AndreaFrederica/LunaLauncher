// SPDX-License-Identifier: MPL-2.0
import 'vue';
declare module '*.vue';

// Vue's template compiler treats accessibility/data attributes and native
// event modifiers as part of a component's public surface.  Quasar's generated
// prop declarations intentionally omit those fall-through attributes, which
// made strict template checking report false positives for every wrapper
// component.  Keep the check strict for actual script/type errors while
// allowing Vue's standard fall-through attributes.
declare module 'vue' {
  interface ComponentCustomProps {
    [key: string]: unknown;
  }
  interface GlobalDirectives {
    vClosePopup: unknown;
  }
  interface HTMLAttributes {
    'data-setup-field'?: string;
    'data-window'?: string;
    'data-tauri-drag-region'?: boolean | string;
    'data-testid'?: string;
  }
  interface InputHTMLAttributes { webkitdirectory?: boolean | string; }
}

declare module '@vue/runtime-dom' {
  interface HTMLAttributes {
    'data-setup-field'?: string;
    'data-window'?: string;
    'data-tauri-drag-region'?: boolean | string;
    'data-testid'?: string;
  }
  interface InputHTMLAttributes { webkitdirectory?: boolean | string; }
}
