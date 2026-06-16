<script setup>
const sizes = {
  sm: 'h-8',
  md: 'h-12',
  lg: 'h-16',
};

const knownGestures = [
  'u', 'd', 'r', 'l', 'ud', 'du', 'dr', 'dl',
  'rd', 'urd', 'ldr', 'urdl', 'ldrdl', 'uldr',
];

const props = defineProps({
  id: {
    type: String,
    required: true,
  },
  size: {
    type: String,
    default: 'md',
    validator: (v) => ['sm', 'md', 'lg'].includes(v),
  },
})

const isKnown = computed(() => knownGestures.includes(props.id.toLowerCase()))
</script>

<template>
  <img
    v-if="isKnown"
    :src="`/img/gestures/gesture_${id.toLowerCase()}.svg`"
    :alt="`Gesture ${id}`"
    class="inline-block align-middle w-auto"
    :class="sizes[size]"
  />
  <span v-else class="inline-block align-middle font-mono text-xs text-red-500">
    {{ `Unknown gesture: ${id}` }}
  </span>
</template>