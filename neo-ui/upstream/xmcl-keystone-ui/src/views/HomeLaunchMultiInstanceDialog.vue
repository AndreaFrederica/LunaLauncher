<template>
  <v-dialog
    v-model="isShown"
    :width="500"
    persistent
  >
    <v-alert
      dense
      border="start"
      type="warning"
      prominent
    >
      <v-card
        elevation="0"
        variant="text"
        color="transparent"
        :title="t('HomeLaunchMultiInstanceDialog.title')"
        :text="t('HomeLaunchMultiInstanceDialog.description')"
      >
        <v-card-actions>
          <v-btn
            @click="isShown = false"
           variant="text">
            {{ t('shared.cancel') }}
          </v-btn>
          <v-spacer />
          <v-btn
            @click="launch(); isShown = false"
           variant="text">
            {{ t('HomeLaunchMultiInstanceDialog.confirm') }}
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-alert>
  </v-dialog>
</template>

<script lang=ts setup>
import { kInstanceLaunch } from '@/composables/instanceLaunch'
import { injection } from '@/util/inject'
import { useDialog } from '../composables/dialog'

const { t } = useI18n()
const { isShown } = useDialog('multi-instance-launch')
const { launch } = injection(kInstanceLaunch)
</script>
