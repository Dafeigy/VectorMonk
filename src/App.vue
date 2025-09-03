<script setup>
document.addEventListener('DOMContentLoaded', function () {
  const connectBtn = document.getElementById('connect-btn');
  const disconnectBtn = document.getElementById('disconnect-btn');
  const statusMessage = document.getElementById('status-message');
  const deviceName = document.getElementById('device-name');
  const dataContent = document.getElementById('data-content');
  const errorContainer = document.getElementById('error-container');
  const errorContent = document.getElementById('error-content');

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
    showError("浏览器不支持Web Bluetooth API。请使用Chrome、Edge或Opera浏览器。");
    connectBtn.disabled = true;
    return;
  }

  // 显示错误信息
  function showError(message) {
    errorContent.textContent = message;
    errorContainer.style.display = 'block';
    statusMessage.textContent = "错误";
    statusMessage.className = "disconnected";
  }

  // 隐藏错误信息
  function hideError() {
    errorContainer.style.display = 'none';
  }

  // 连接设备
  connectBtn.addEventListener('click', async function () {
    if (isConnecting) return;

    isConnecting = true;
    hideError();
    connectBtn.disabled = true;
    statusMessage.textContent = "正在扫描设备...";
    statusMessage.className = "scanning";

    try {
      // 请求蓝牙设备，筛选ESP32设备
      device = await navigator.bluetooth.requestDevice({
        filters: [
          { namePrefix: "[NUL4i]" },
        ],
        optionalServices: ['4fafc201-1fb5-459e-8fcc-c5c9c331914b']
      });

      statusMessage.textContent = "设备找到，正在连接...";
      deviceName.textContent = device.name || "未知设备";

      // 监听设备断开事件
      device.addEventListener('gattserverdisconnected', onDisconnected);

      // 连接到GATT服务器
      await connectToDevice();

    } catch (error) {
      console.error('连接错误:', error);
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
      service = await server.getPrimaryService('4fafc201-1fb5-459e-8fcc-c5c9c331914b');

      statusMessage.textContent = "服务已找到，正在获取特征...";

      // 获取特征
      characteristic = await service.getCharacteristic('beb5483e-36e1-4688-b7f5-ea07361b26a8');

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
      console.error('连接错误:', error);

      // 如果是断开错误，尝试重新连接
      if (error.message.includes('disconnected') && reconnectAttempts < maxReconnectAttempts) {
        reconnectAttempts++;
        statusMessage.textContent = `尝试重新连接 (${reconnectAttempts}/${maxReconnectAttempts})...`;

        // 等待一段时间后重试
        await new Promise(resolve => setTimeout(resolve, 1000));
        return connectToDevice();
      }

      throw error;
    }
  }

  // 断开连接
  disconnectBtn.addEventListener('click', function () {
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
      device.removeEventListener('gattserverdisconnected', onDisconnected);
    }
  }

  // 处理连接错误
  function handleConnectionError(error) {
    let errorMsg = "连接错误: " + error.message;

    if (error.message.includes('disconnected')) {
      errorMsg += "\n\n解决方法:\n1. 确保ESP32设备已上电并在广播\n2. 尝试重新连接设备\n3. 检查设备是否在蓝牙范围内";
    } else if (error.message.includes('user cancelled')) {
      errorMsg = "用户取消了设备选择";
    } else if (error.message.includes('not found')) {
      errorMsg = "未找到设备。请确保:\n1. ESP32设备已上电\n2. 设备名以'ESP32'开头\n3. 设备在蓝牙范围内";
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
      const decoder = new TextDecoder('utf-8');
      const message = decoder.decode(value);

      // 显示数据，添加时间戳
      const now = new Date();
      const timestamp = now.toLocaleTimeString();
      dataContent.textContent = `[${timestamp}] \n${message}`;

      // 限制显示的行数
      const lines = dataContent.textContent.split('\n');
      if (lines.length > 20) {
        dataContent.textContent = lines.slice(0, 20).join('\n');
      }

    } catch (error) {
      console.error('读取数据错误:', error);

      // 如果是断开错误，尝试重新连接
      if (error.message.includes('disconnected') && reconnectAttempts < maxReconnectAttempts) {
        reconnectAttempts++;
        statusMessage.textContent = `连接断开，尝试重新连接 (${reconnectAttempts}/${maxReconnectAttempts})...`;

        // 尝试重新连接
        try {
          await connectToDevice();
        } catch (reconnectError) {
          console.error('重连错误:', reconnectError);
          handleConnectionError(reconnectError);
        }
      } else if (error.message.includes('disconnected')) {
        handleConnectionError(error);
      }
    }
  }
});
// import { ref, onMounted, onUnmounted } from "vue";
// import * as THREE from "three";

// const canvasContainer = ref(null);
// let scene, camera, renderer, cube;

// // 初始化场景
// function initScene() {
//   // 1. 创建场景
//   scene = new THREE.Scene();
//   scene.background = new THREE.Color(0x333333);
//   // 2. 创建相机（透视相机）
//   camera = new THREE.PerspectiveCamera(
//     75,
//     window.innerWidth / window.innerHeight,
//     0.1,
//     1000
//   );
//   camera.position.z = 5;
//   let container = document.getElementById("canvasContainer");
//   const width = container.clientWidth;
//   const height = container.clientHeight;
//   // 3. 创建渲染器
//   renderer = new THREE.WebGLRenderer({ antialias: true });
//   renderer.setSize(width, height);
//   document.getElementById("canvasContainer").appendChild(renderer.domElement);
//   // 4. 挂载到DOM
//   // canvasContainer.value.appendChild(renderer.domElement);
// }

// // 创建立方体
// function createCube() {
//   const geometry = new THREE.BoxGeometry(1, 1, 1);
//   const material = new THREE.MeshBasicMaterial({
//     color: 0x00ff00,
//     wireframe: true,
//   });
//   cube = new THREE.Mesh(geometry, material);
//   scene.add(cube);
// }

// // 动画循环
// function animate() {
//   requestAnimationFrame(animate);

//   cube.rotation.x += 0.01;
//   cube.rotation.y += 0.01;

//   renderer.render(scene, camera);
// }

// const connectToBluetooth = async () => {
//   try {
//     // 请求蓝牙设备，指定服务UUID
//     this.device = await navigator.bluetooth.requestDevice({
//       filters: [
//         { services: ['4fafc201-1fb5-459e-8fcc-c5c9c331914b'] }
//       ]
//     });

//     // 连接GATT服务器
//     this.server = await this.device.gatt.connect();

//     // 获取服务
//     const service = await this.server.getPrimaryService('4fafc201-1fb5-459e-8fcc-c5c9c331914b'.toLowerCase());

//     // 获取特征（这里使用常见的Nordic UART的RX特征UUID，用于通知）
//     this.characteristic = await service.getCharacteristic('beb5483e-36e1-4688-b7f5-ea07361b26a8'.toLowerCase());

//     // 开启通知
//     await this.characteristic.startNotifications();

//     // 监听特征值变化
//     this.characteristic.addEventListener('characteristicvaluechanged', this.handleNotifications);

//     // 将连接成功的信息显示出来
//     this.messages.push('已连接蓝牙设备，开始监听通知...');
//   } catch (error) {
//     console.error('连接蓝牙失败:', error);
//   }
// };

// // 处理通知事件
// const handleNotifications = (event) => {
//   const value = event.target.value;
//   // 解析数据，假设是文本数据（UTF-8）
//   const decoder = new TextDecoder('utf-8');
//   const message = decoder.decode(value);
//   this.messages.push(message);
// };

// // 在组件销毁时断开连接
// function beforeUnmount() {
//   if (this.characteristic) {
//     this.characteristic.removeEventListener('characteristicvaluechanged', this.handleNotifications);
//     this.characteristic.stopNotifications();
//   }
//   if (this.server) {
//     this.server.disconnect();
//   }
// };

// onMounted(() => {
//   initScene();
//   createCube();
//   animate();
// });

// // 组件卸载时清理资源
// onUnmounted(() => {
//   if (renderer) {
//     renderer.dispose();
//     canvasContainer.value.removeChild(renderer.domElement);
//   }
// });
</script>

<template>
  <div id="header" class="h-[10%] bg-[#1c1c1e] w-full rounded-t-2xl"></div>
  <div id="middle" class="w-full h-[85%] flex justify-between">
    <div id="left" class="w-[15%] bg-[#1b1b1c] h-full flex justify-center items-center">
    </div>
    <div id="right" class="w-full bg-[#1a1a1b] h-full flex justify-center items-center">
      <div id="canvasContainer" class="h-[70%] w-[70%]  flex justify-center items-center rounded-2xl">
        <div class="container">
          <div class="content">
            <div class="status">
              <div class="status-icon">📶</div>
              <div class="status-text text-white">
                <h3>状态: <span id="status-message" >等待连接</span></h3>
                <p>设备: <span id="device-name">未连接</span></p>
              </div>
            </div>

            <div class="button-container">
              <button id="connect-btn" class="connect-btn">
                <span>连接设备</span>
              </button>
              <button id="disconnect-btn" class="disconnect-btn" disabled>
                <span>断开连接</span>
              </button>
            </div>

            <div class="data-container">
              <div class="data-title">
                <span>[NUL4i@ESP32S3]</span>
              </div>
              <div id="data-content" class="data-content">
                // 数据将显示在这里...
              </div>
            </div>

            <div id="error-container" class="error-container">
              <h3>错误信息</h3>
              <div id="error-content" class="error-content"></div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
  <div id="bottom" class="w-full bg-[#1c1c1e] h-[5%] rounded-b-2xl"></div>
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

* {
  box-sizing: border-box;
  margin: 0;
  padding: 0;
  font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
}

body {
  background: #151517;
  color: #333;
  min-height: 100vh;
  display: flex;
  justify-content: center;
  align-items: center;
  padding: 20px;
}

.container {
  width: 100%;
  max-width: 600px;
  /* background-color: rgba(255, 255, 255, 0.92); */
  border-radius: 20px;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.2);
  overflow: hidden;
}

.content {
  padding: 25px;
}

.status {
  /* background-color: #f8f9fa; */
  border-left: 4px solid salmon;
  padding: 15px;
  margin-bottom: 25px;
  border-radius: 4px;
  display: flex;
  align-items: center;
}

.status-icon {
  font-size: 24px;
  margin-right: 15px;
}

.status-text {
  flex: 1;
}

.button-container {
  display: flex;
  gap: 15px;
  margin-bottom: 25px;
}

button {
  flex: 1;
  padding: 15px;
  border: none;
  border-radius: 8px;
  font-weight: 600;
  font-size: 16px;
  cursor: pointer;
  transition: all 0.3s ease;
  display: flex;
  justify-content: center;
  align-items: center;
}

button:disabled {
  opacity: 0.6;
  cursor: not-allowed;
}

.connect-btn {
  background-color: rgb(27, 139, 121);
  color: white;
}

.connect-btn:hover:not(:disabled) {
  background-color: #27ae60;
}

.disconnect-btn {
  background-color: #ad382b;
  color: white;
}

.disconnect-btn:hover:not(:disabled) {
  background-color: #c0392b;
}

.data-container {
  /* background-color: #f8f9fa; */
  border-radius: 8px;
  padding: 20px;
  margin-bottom: 20px;
}

.data-title {
  font-size: 18px;
  font-weight: 600;
  margin-bottom: 15px;
  color: #eee;
  display: flex;
  align-items: center;
}

.data-title span {
  margin-left: 10px;
}

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

.instructions {
  background-color: #fff8e1;
  border-left: 4px solid #ffc107;
  padding: 15px;
  border-radius: 4px;
  font-size: 14px;
}

.instructions h3 {
  margin-bottom: 10px;
  color: #ff9800;
}

.instructions ul {
  padding-left: 20px;
}

.instructions li {
  margin-bottom: 8px;
}

.blink {
  animation: blink 1s infinite;
}

@keyframes blink {
  50% {
    opacity: 0.5;
  }
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
