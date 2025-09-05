<script setup>
document.addEventListener("DOMContentLoaded", function () {
  const connectBtn = document.getElementById("connect-btn");
  const disconnectBtn = document.getElementById("disconnect-btn");
  const statusMessage = document.getElementById("status-message");
  const deviceName = document.getElementById("device-name");
  const dataContent = document.getElementById("data-content");
  const errorContainer = document.getElementById("error-container");
  const errorContent = document.getElementById("error-content");

  let device = null;
  let server = null;
  let service = null;
  let characteristic = null;
  let readInterval = null;
  let isConnecting = false;
  let reconnectAttempts = 0;
  const maxReconnectAttempts = 3;

  // 检查浏览器是否支持Web Bluetooth API
  if (!navigator.bluetooth) {
    showError(
      "浏览器不支持Web Bluetooth API。请使用Chrome、Edge或Opera浏览器。"
    );
    connectBtn.disabled = true;
    return;
  }

  // 显示错误信息
  function showError(message) {
    errorContent.textContent = message;
    errorContainer.style.display = "block";
    statusMessage.textContent = "错误";
    statusMessage.className = "disconnected";
  }

  // 隐藏错误信息
  function hideError() {
    errorContainer.style.display = "none";
  }

  // 连接设备
  connectBtn.addEventListener("click", async function () {
    if (isConnecting) return;

    isConnecting = true;
    hideError();
    connectBtn.disabled = true;
    statusMessage.textContent = "正在扫描设备...";
    statusMessage.className = "scanning";

    try {
      // 请求蓝牙设备，筛选ESP32设备
      device = await navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: "[NUL4i]" }],
        optionalServices: ["4fafc201-1fb5-459e-8fcc-c5c9c331914b"],
      });

      statusMessage.textContent = "设备找到，正在连接...";
      deviceName.textContent = device.name || "未知设备";

      // 监听设备断开事件
      device.addEventListener("gattserverdisconnected", onDisconnected);

      // 连接到GATT服务器
      await connectToDevice();
    } catch (error) {
      console.error("连接错误:", error);
      handleConnectionError(error);
    } finally {
      isConnecting = false;
    }
  });

  // 连接到设备
  async function connectToDevice() {
    try {
      if (!device) {
        throw new Error("没有可用的设备");
      }

      // 检查是否已经连接
      if (device.gatt && device.gatt.connected) {
        server = device.gatt;
      } else {
        // 连接到GATT服务器
        server = await device.gatt.connect();
      }

      statusMessage.textContent = "已连接，正在获取服务...";

      // 获取服务
      service = await server.getPrimaryService(
        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
      );

      statusMessage.textContent = "服务已找到，正在获取特征...";

      // 获取特征
      characteristic = await service.getCharacteristic(
        "beb5483e-36e1-4688-b7f5-ea07361b26a8"
      );

      statusMessage.textContent = "已连接，等待数据...";
      statusMessage.className = "connected";
      connectBtn.disabled = true;
      disconnectBtn.disabled = false;

      // 重置重连尝试次数
      reconnectAttempts = 0;

      // 开始读取数据
      readData();
      // 设置定时读取
      if (readInterval) clearInterval(readInterval);
      readInterval = setInterval(readData, 30);
    } catch (error) {
      console.error("连接错误:", error);

      // 如果是断开错误，尝试重新连接
      if (
        error.message.includes("disconnected") &&
        reconnectAttempts < maxReconnectAttempts
      ) {
        reconnectAttempts++;
        statusMessage.textContent = `尝试重新连接 (${reconnectAttempts}/${maxReconnectAttempts})...`;

        // 等待一段时间后重试
        await new Promise((resolve) => setTimeout(resolve, 1000));
        return connectToDevice();
      }

      throw error;
    }
  }

  // 断开连接
  disconnectBtn.addEventListener("click", function () {
    if (device && device.gatt.connected) {
      device.gatt.disconnect();
    } else {
      onDisconnected();
    }
  });

  // 断开连接处理
  function onDisconnected() {
    statusMessage.textContent = "设备已断开连接";
    statusMessage.className = "disconnected";
    deviceName.textContent = "未连接";
    connectBtn.disabled = false;
    disconnectBtn.disabled = true;

    if (readInterval) {
      clearInterval(readInterval);
      readInterval = null;
    }

    if (device) {
      device.removeEventListener("gattserverdisconnected", onDisconnected);
    }
  }

  // 处理连接错误
  function handleConnectionError(error) {
    let errorMsg = "连接错误: " + error.message;

    if (error.message.includes("disconnected")) {
      errorMsg +=
        "\n\n解决方法:\n1. 确保ESP32设备已上电并在广播\n2. 尝试重新连接设备\n3. 检查设备是否在蓝牙范围内";
    } else if (error.message.includes("user cancelled")) {
      errorMsg = "用户取消了设备选择";
    } else if (error.message.includes("not found")) {
      errorMsg =
        "未找到设备。请确保:\n1. ESP32设备已上电\n2. 设备名以'ESP32'开头\n3. 设备在蓝牙范围内";
    }

    showError(errorMsg);
    statusMessage.textContent = "连接失败";
    statusMessage.className = "disconnected";
    connectBtn.disabled = false;
  }

  // 读取数据
  async function readData() {
    if (!characteristic) return;

    try {
      const value = await characteristic.readValue();
      const decoder = new TextDecoder("utf-8");
      const message = decoder.decode(value);

      // 显示数据，添加时间戳
      const now = new Date();
      const timestamp = now.toLocaleTimeString();
      dataContent.textContent = `[${timestamp}] \n${message}`;

      // 限制显示的行数
      const lines = dataContent.textContent.split("\n");
      if (lines.length > 20) {
        dataContent.textContent = lines.slice(0, 20).join("\n");
      }
    } catch (error) {
      console.error("读取数据错误:", error);

      // 如果是断开错误，尝试重新连接
      if (
        error.message.includes("disconnected") &&
        reconnectAttempts < maxReconnectAttempts
      ) {
        reconnectAttempts++;
        statusMessage.textContent = `连接断开，尝试重新连接 (${reconnectAttempts}/${maxReconnectAttempts})...`;

        // 尝试重新连接
        try {
          await connectToDevice();
        } catch (reconnectError) {
          console.error("重连错误:", reconnectError);
          handleConnectionError(reconnectError);
        }
      } else if (error.message.includes("disconnected")) {
        handleConnectionError(error);
      }
    }
  }
});
import { ref, onMounted, onUnmounted } from "vue";
import * as THREE from "three";

const canvasContainer = ref(null);
let scene, camera, renderer, cube;

// 初始化场景
function initScene() {
  // 1. 创建场景
  scene = new THREE.Scene();
  scene.background = null;
  // 2. 创建相机（透视相机）

  let container = document.getElementById("canvasContainer");
  const width = container.clientWidth;
  const height = container.clientHeight;
  camera = new THREE.PerspectiveCamera(
    75,
    width / height,
    0.1,
    1000
  );
  camera.position.z = 5;
  // 3. 创建渲染器
  renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
  renderer.setSize(width, height);
  renderer.setClearAlpha(0);
  document.getElementById("canvasContainer").appendChild(renderer.domElement);
}

// 创建立方体
function createCube() {
  const geometry = new THREE.BoxGeometry(1.5, 1.5, 1.5);
  const material = new THREE.MeshLambertMaterial({
    color: 0xf1a6b9,
  });
  cube = new THREE.Mesh(geometry, material);
  const spotLight = new THREE.SpotLight(0xffffff, 1500);
  spotLight.position.set(10, 10, 15);
  scene.add(spotLight);

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
  <div
    id="main"
    class="font-pixel shadow-2xl bg-gradient-to-r from-[#e0bf78] via-[#e6bd8f] to-[#f1a6b9] dark:bg-gradient-to-r dark:from-[#29323c] dark:via-[#485563] dark:to-[#29323c] flex flex-col aspect-16-9 h-full justify-center items-center rounded-2xl border-1 border-light-border dark:border-dark-border"
  >
    <div
      id="header"
      class="border-light-border dark:border-dark-border bg-light-glass h-[10%] w-full backdrop-blur-md rounded-t-2xl flex justify-center items-center"
    >
      <div class="flex ml-2 items-center w-full h-full">
        <div class="w-full h-full flex items-center">
          <button
            id="connect-btn"
            class="shadow-2xl border-1 border-transparent dark:hover:border-dark-border text-light-text aspect-square flex items-center justify-center hover:scale-105 cursor-pointer transition-all ease-in-out duration-300 h-3/5 rounded-xl mx-2 dark:hover:bg-light-glass hover:bg-light-glass"
          >
            <span class="text-light-text w-[30%]"
              ><svg
                xmlns="http://www.w3.org/2000/svg"
                xmlns:xlink="http://www.w3.org/1999/xlink"
                viewBox="0 0 24 24"
                class=""
              >
                <g fill="none">
                  <path
                    d="M10.004 9.608V3.753c0-.646.752-.974 1.225-.58l.076.072l4.712 5.104a.75.75 0 0 1 .005 1.012l-.076.073l-3.077 2.564l3.077 2.566a.75.75 0 0 1 .137 1.002l-.066.083l-4.711 5.106c-.44.476-1.214.207-1.295-.404l-.007-.104v-5.863l-1.12.934a.75.75 0 0 1-1.04-1.075l.08-.077l2.08-1.734v-.87l-2.08-1.736a.75.75 0 0 1 .87-1.217l.09.065l1.12.934V3.753v5.855zm1.694 3.366l-.194.16v5.194l2.876-3.117l-2.682-2.237zM17 11a1 1 0 1 1 0 2a1 1 0 0 1 0-2zM6 11a1 1 0 1 1 0 2a1 1 0 0 1 0-2zm5.504-5.328v5.187l.194.162l2.682-2.234l-2.876-3.115z"
                    fill="currentColor"
                  ></path>
                </g>
              </svg>
            </span>
            <!-- <p class="text-sm mx-2 icontexthide">Connect</p> -->
          </button>
          <button
            class="shadow-2xl border-1 border-transparent dark:hover:border-dark-border text-light-text aspect-square flex items-center justify-center hover:scale-105 cursor-pointer transition-all ease-in-out duration-300 h-3/5 rounded-xl mx-2 dark:hover:bg-light-glass hover:bg-light-glass"
          >
            <span class="text-light-text w-[30%]">
              <svg
                xmlns="http://www.w3.org/2000/svg"
                xmlns:xlink="http://www.w3.org/1999/xlink"
                viewBox="0 0 24 24"
              >
                <path
                  d="M9 19c-4.3 1.4-4.3-2.5-6-3m12 5v-3.5c0-1 .1-1.4-.5-2c2.8-.3 5.5-1.4 5.5-6a4.6 4.6 0 0 0-1.3-3.2a4.2 4.2 0 0 0-.1-3.2s-1.1-.3-3.5 1.3a12.3 12.3 0 0 0-6.2 0C6.5 2.8 5.4 3.1 5.4 3.1a4.2 4.2 0 0 0-.1 3.2A4.6 4.6 0 0 0 4 9.5c0 4.6 2.7 5.7 5.5 6c-.6.6-.6 1.2-.5 2V21"
                  fill="none"
                  stroke="currentColor"
                  stroke-width="2"
                  stroke-linecap="round"
                  stroke-linejoin="round"
                ></path>
              </svg>
            </span>
            <!-- <p class="text-sm mx-2 icontexthide">Home</p> -->
          </button>
          <button
            id="disconnect-btn"
            class="shadow-2xl border-1 border-transparent dark:hover:border-dark-border text-light-text aspect-square flex items-center justify-center hover:scale-105 cursor-pointer transition-all ease-in-out duration-300 h-3/5 rounded-xl mx-2 dark:hover:bg-light-glass hover:bg-light-glass"
            disabled
          >
            <span class="text-light-text w-[30%]"
              ><svg
                xmlns="http://www.w3.org/2000/svg"
                xmlns:xlink="http://www.w3.org/1999/xlink"
                viewBox="0 0 24 24"
              >
                <g
                  fill="none"
                  stroke="currentColor"
                  stroke-width="2"
                  stroke-linecap="round"
                  stroke-linejoin="round"
                >
                  <path d="M12 21V3"></path>
                  <path d="M15 6l-3-3l-3 3"></path>
                  <path d="M9 21h6"></path>
                </g>
              </svg>
            </span>
          </button>
        </div>
      </div>
      <div
        class="text-2xl text-center flex absolute dark:text-white text-light-text"
      >
        [VectorMonk]
      </div>
    </div>
    <div
      id="middle"
      class="w-full h-[85%] flex-col justify-center items-center flex-1 overflow-y-auto no-scrollbar"
    >
      <div
        class="second w-full h-1/2 text-2xl text-center"
        id="canvasContainer"
      ></div>
      <div
        class="h-1/3 w-full flex justify-center items-center rounded-2xl flex-col "
      >
        <div
          class="qmi-screen-container bg-white/30 max-w-[450px] w-full rounded-2xl shadow-2xl overflow-hidden dark:bg-[rgba(3,3,3,0.3)]"
        >
          <div class="p-[25px]">
            <div class="rounded-xl dark:bg-[#2D3133] bg-[#252627] ">
              <div
                class="data-title dark:text-light-white text-light-text flex items-center text-2xl mb-2"
              >
                <span class="text-white px-2 text-sm">[NUL4i@ESP32S3]</span>
              </div>
              <div
                id="data-content"
                class="text-xs rounded-xl whitespace-pre-wrap dark:text-[#ecf0f1] text-light-bg px-2 overflow-y-auto min-h-[100px] max-h-[200px] dark:bg-[#2D3133] bg-[#252627]"
              >
                数据将显示在这里...<br></br><br></br>
                ACCEL<br></br>
                64.0,32.1,23.1<br></br>
                GYRO<br></br>
                64.0,32.1,23.1<br></br>
              </div>
            </div>

            <div id="error-container" class="error-container hidden">
              <h3>错误信息</h3>
              <div id="error-content" class="error-content"></div>
            </div>
          </div>
        </div>
      </div>

      <div
        class="third w-full h-full text-2xl text-center items-center justify-center flex"
      >
        NUL4i x T3-ESP32-DISPLAY
      </div>
    </div>
    <div
      id="bottom"
      class="w-full h-[5%] rounded-b-2xl bg-light-glass backdrop-blur-md flex items-center"
    >
      <div id="device-name" class="mx-4 z-100 text-[#333]">
        设备名称：<span class="text-[#e0904e]">未知</span>
      </div>
      <div id="status-message" class="mx-4 z-100 text-[#333]">
        连接状态：<span class="text-[#e0904e]">未连接</span>
      </div>
      <div
        id="copyright"
        class="absolute left-1/2 transform -translate-x-1/2 z-100 text-[#3b3b3b]"
      >
        NUL4i@2025-2026
      </div>
    </div>
  </div>
</template>

<style>
.data-content {
  min-height: 100px;
  max-height: 200px;
  overflow-y: auto;
  padding: 15px;
  background-color: #252627;
  color: #ecf0f1;
  border-radius: 8px;
  font-family: monospace;
  white-space: pre-wrap;
}

.connected {
  color: #2ecc71;
}

.disconnected {
  color: #e74c3c;
}

.scanning {
  color: #3498db;
}

@media (max-width: 600px) {
  .button-container {
    flex-direction: column;
  }

  .header h1 {
    font-size: 24px;
  }
}

#app {
  width: 80%;
  height: 80%;
}
</style>
