export interface PclNavItem {
  id: string;
  label: string;
  icon: string;
  refresh?: boolean;
}

export interface PclNavGroup {
  label?: string;
  items: PclNavItem[];
}

export interface PclSelectOption {
  label: string;
  value: string | number;
  disabled?: boolean;
}
