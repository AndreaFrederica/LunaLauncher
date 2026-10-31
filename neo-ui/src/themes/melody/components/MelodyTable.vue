<template>
  <div class="melody-table">
    <table :aria-label="label"><thead><tr><th v-for="column in columns" :key="column.key" :style="{ width: column.width }">{{ column.label }}</th></tr></thead>
      <tbody><tr v-for="row in rows" :key="row.id" :class="{ 'is-selected': modelValue === row.id }" tabindex="0" :aria-selected="modelValue === row.id" @click="$emit('update:modelValue', row.id)" @keydown.enter="$emit('update:modelValue', row.id)" @keydown.space.prevent="$emit('update:modelValue', row.id)"><td v-for="column in columns" :key="column.key">{{ row[column.key] }}</td></tr></tbody>
    </table>
    <p v-if="!rows.length && emptyText" class="melody-table__empty">{{ emptyText }}</p>
  </div>
</template>
<script setup lang="ts">
defineProps<{ label: string; columns: { key: string; label: string; width?: string }[]; rows: { id: string; [key: string]: string }[]; modelValue?: string; emptyText?: string }>();
defineEmits<{ 'update:modelValue': [value: string] }>();
</script>
