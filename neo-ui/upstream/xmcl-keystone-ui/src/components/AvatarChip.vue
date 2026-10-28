<template>
  <v-chip
    label
    variant="outlined"
    :size="small ? 'small' : undefined"
    @click="emit('click')"
  >
    <template #prepend>
      <v-avatar start>
        <img
          :src="avatar"
        >
      </v-avatar>
    </template>
    <div v-if="text">
      {{ text }}
    </div>
  </v-chip>
</template>
<script lang="ts" setup>
defineProps<{
  small?: boolean
  avatar?: string
  text?: string
}>()
const emit = defineEmits(['click'])
</script>
