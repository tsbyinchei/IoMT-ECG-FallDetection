/**
 * IoMT Telehealth Patient Monitor - Service Worker (PWA Offline & Cache Engine)
 * Cache Shell Assets and handle offline capabilities.
 */

const CACHE_NAME = 'iomt-monitor-v1.1';
const STATIC_ASSETS = [
  './',
  './index.html',
  './style.css',
  './app.js',
  './manifest.json',
  './assets/icon.ico',
  './assets/logo.webp',
  './assets/splash.webp',
  './assets/icon-48.webp',
  './assets/icon-72.webp',
  './assets/icon-96.webp',
  './assets/icon-128.webp',
  './assets/icon-192.webp',
  './assets/icon-256.webp',
  './assets/icon-512.webp'
];

// 1. Cài đặt Service Worker (Cache Static Assets)
self.addEventListener('install', (event) => {
  event.waitUntil(
    caches.open(CACHE_NAME).then((cache) => {
      console.log('[PWA SW] Pre-caching static assets...');
      return cache.addAll(STATIC_ASSETS);
    }).then(() => self.skipWaiting())
  );
});

// 2. Kích hoạt và dọn dẹp Cache cũ
self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((cacheNames) => {
      return Promise.all(
        cacheNames.map((name) => {
          if (name !== CACHE_NAME) {
            console.log('[PWA SW] Removing old cache:', name);
            return caches.delete(name);
          }
        })
      );
    }).then(() => self.clients.claim())
  );
});

// 3. Xử lý yêu cầu tài nguyên (Network First cho code, Cache First cho ảnh/assets)
self.addEventListener('fetch', (event) => {
  const request = event.request;

  // Bỏ qua các yêu cầu WebSocket (wss://) hoặc không phải HTTP GET
  if (request.method !== 'GET' || request.url.startsWith('ws://') || request.url.startsWith('wss://')) {
    return;
  }

  // Đối với hình ảnh và fonts: Cache First
  if (request.destination === 'image' || request.destination === 'font') {
    event.respondWith(
      caches.match(request).then((cachedResponse) => {
        if (cachedResponse) return cachedResponse;
        return fetch(request).then((networkResponse) => {
          if (networkResponse && networkResponse.status === 200) {
            const responseClone = networkResponse.clone();
            caches.open(CACHE_NAME).then((cache) => cache.put(request, responseClone));
          }
          return networkResponse;
        });
      })
    );
    return;
  }

  // Đối với HTML, CSS, JS: Network First, fallback về Cache nếu offline
  event.respondWith(
    fetch(request)
      .then((networkResponse) => {
        if (networkResponse && networkResponse.status === 200) {
          const responseClone = networkResponse.clone();
          caches.open(CACHE_NAME).then((cache) => cache.put(request, responseClone));
        }
        return networkResponse;
      })
      .catch(() => {
        return caches.match(request).then((cachedResponse) => {
          if (cachedResponse) return cachedResponse;
          if (request.mode === 'navigate') {
            return caches.match('./index.html');
          }
        });
      })
  );
});
