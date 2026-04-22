import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

export default defineConfig({
  plugins: [react()],
  server: {
    port: 5173,
    proxy: {
      '/sim': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/process': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/scheduler': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/memory': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/workload': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/sync': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/state': {
        target: 'http://localhost:8080',
        changeOrigin: true,
      },
      '/ws': {
        target: 'http://localhost:8080',
        ws: true,
        changeOrigin: true,
      },
    },
  },
})
