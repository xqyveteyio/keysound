import { createRouter, createWebHashHistory } from 'vue-router'
import HomeView from '../views/KeyBoardView.vue'

const routes = [
  {
    path: '/',
    redirect: '/keyboard'
  },
  {
    path: '/keyboard',
    name: 'keyboard',
    component: () => import(/* webpackChunkName: "keyboard" */ '../views/KeyBoardView.vue')
  },
  {
    path: '/mouse',
    name: 'mouse',
    component: () => import(/* webpackChunkName: "mouse" */ '../views/MouseView.vue')
  },
  {
    path: '/sounds',
    name: 'sounds',
    component: () => import(/* webpackChunkName: "sounds" */ '../views/SoundsView.vue')
  },
  {
    path: '/setup',
    name: 'setup',
    component: () => import(/* webpackChunkName: "setup" */ '../views/SetupView.vue')
  },
  {
    path: '/about',
    name: 'about',
    component: () => import(/* webpackChunkName: "about" */ '../views/AboutView.vue')
  },
  {
    path: '/vuex',
    name: 'vuex',
    component: () => import('@/views/VUEX.vue')
  },

]

const router = createRouter({
  history: createWebHashHistory(),
  routes
})

export default router
