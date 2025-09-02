<script setup>
import { ref, onMounted, onUnmounted } from "vue";
import * as THREE from "three";

const canvasContainer = ref(null);
let scene, camera, renderer, cube;

// 初始化场景
function initScene() {
  // 1. 创建场景
  scene = new THREE.Scene();
  scene.background = new THREE.Color(0x333333);
  // 2. 创建相机（透视相机）
  camera = new THREE.PerspectiveCamera(
    75,
    window.innerWidth / window.innerHeight,
    0.1,
    1000
  );
  camera.position.z = 5;
  let container = document.getElementById("canvasContainer");
  const width = container.clientWidth;
  const height = container.clientHeight;
  // 3. 创建渲染器
  renderer = new THREE.WebGLRenderer({ antialias: true });
  renderer.setSize(width, height);
  document.getElementById("canvasContainer").appendChild(renderer.domElement);
  // 4. 挂载到DOM
  // canvasContainer.value.appendChild(renderer.domElement);
}

// 创建立方体
function createCube() {
  const geometry = new THREE.BoxGeometry(1, 1, 1);
  const material = new THREE.MeshBasicMaterial({
    color: 0x00ff00,
    wireframe: true,
  });
  cube = new THREE.Mesh(geometry, material);
  scene.add(cube);
}

// 动画循环
function animate() {
  requestAnimationFrame(animate);

  cube.rotation.x += 0.01;
  cube.rotation.y += 0.01;

  renderer.render(scene, camera);
}

onMounted(() => {
  initScene();
  createCube();
  animate();
});

// 组件卸载时清理资源
onUnmounted(() => {
  if (renderer) {
    renderer.dispose();
    canvasContainer.value.removeChild(renderer.domElement);
  }
});
</script>

<template>
  <div id="header" class="h-[10%] bg-indigo-300 w-full">3</div>
  <div id="middle" class="w-full h-[85%] bg-green-300 flex justify-between">
    <div id="left" class="w-[15%] bg-amber-100 h-full">2</div>
    <div
      id="right"
      class="w-full bg-blue-300 h-full flex justify-center items-center"
    >
      <div id="canvasContainer" class="h-[70%] w-[70%] bg-green-300"></div>
    </div>
  </div>
  <div id="bottom" class="w-full bg-orange-400 h-[5%]">0</div>
</template>

<style>
body {
  padding: 0;
  margin: 0;
  width: 100vw;
  height: 100vh;
  display: flex;
  justify-content: center;
  align-items: center;
}

#app {
  width: 80%;
  height: 80%;
}
</style>
