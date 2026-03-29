<template>
	<view class="container">
		<view class="header" :class="riskLevel >= 2 ? 'danger-bg' : 'safe-bg'">
			<view class="title-box">
				<text class="title-icon">🔥</text>
				<text class="title-text">智能火情监控中心</text>
			</view>
			<text class="time-text">最后同步: {{ lastUpdateTime }}</text>
		</view>

		<view class="tab-bar">
			<view class="tab-item" :class="{ 'active': currentTab === 0 }" @click="switchTab(0)">综合大盘</view>
			<view class="tab-item" :class="{ 'active': currentTab === 1 }" @click="switchTab(1)">视觉分析</view>
			<view class="tab-item" :class="{ 'active': currentTab === 2 }" @click="switchTab(2)">传感阵列</view>
		</view>

		<swiper class="swiper-box" :current="currentTab" @change="onSwiperChange">
			
			<swiper-item>
				<scroll-view scroll-y class="scroll-page">
					<view class="card-container">
						<view class="data-card main-card" :class="riskLevel >= 2 ? 'danger-text' : 'safe-text'">
							<text class="card-title">⚠️ 综合风险评估</text>
							<text class="card-value">Level {{ riskLevel }}</text>
							<text class="card-desc">{{ riskLevel === 3 ? '致命危险：确认起火！' : (riskLevel === 0 ? '环境安全' : '潜在风险，请注意') }}</text>
						</view>
						<view class="data-card">
							<text class="card-title">🛡️ 综合确信度</text>
							<text class="card-value">{{ confidence }} <text class="unit">%</text></text>
						</view>
						<view class="data-card clickable" @click="switchTab(1)">
							<text class="card-title">📷 图像置信度 👉</text>
							<text class="card-value">{{ imageConfidence }} <text class="unit">%</text></text>
						</view>
					</view>
					
					<view class="status-panel">
						<text class="panel-title">系统检测状态</text>
						<view class="light-group">
							<view class="light-item">
								<view class="led" :class="imageFireDetected ? 'led-red' : 'led-green'"></view>
								<text>视觉检测</text>
							</view>
							<view class="light-item">
								<view class="led" :class="esp32FireDetected ? 'led-red' : 'led-green'"></view>
								<text>ESP32协处理</text>
							</view>
						</view>
					</view>
				</scroll-view>
			</swiper-item>

			<swiper-item>
				<scroll-view scroll-y class="scroll-page">
					<view class="card-container">
						<view class="data-card">
							<text class="card-title">🔥 火焰区域面积</text>
							<text class="card-value">{{ fireArea }} <text class="unit">px</text></text>
						</view>
						<view class="data-card">
							<text class="card-title">📍 火焰中心 X</text>
							<text class="card-value">{{ fireCenterX }} <text class="unit">px</text></text>
						</view>
						<view class="data-card">
							<text class="card-title">📍 火焰中心 Y</text>
							<text class="card-value">{{ fireCenterY }} <text class="unit">px</text></text>
						</view>
					</view>
					
					<view class="status-panel">
						<text class="panel-title">图像特征追踪</text>
						<view class="light-group">
							<view class="light-item">
								<view class="led" :class="flickerDetected ? 'led-red' : 'led-green'"></view>
								<text>边缘闪烁识别</text>
							</view>
						</view>
					</view>
				</scroll-view>
			</swiper-item>

			<swiper-item>
				<scroll-view scroll-y class="scroll-page">
					<view class="card-container">
						<view class="data-card">
							<text class="card-title">💨 烟雾浓度</text>
							<text class="card-value">{{ smokePpm }} <text class="unit">PPM</text></text>
						</view>
						<view class="data-card">
							<text class="card-title">🌡️ 环境温度</text>
							<text class="card-value">{{ temperature }} <text class="unit">°C</text></text>
						</view>
						<view class="data-card">
							<text class="card-title">💧 相对湿度</text>
							<text class="card-value">{{ humidity }} <text class="unit">%</text></text>
						</view>
						<view class="data-card">
							<text class="card-title">⚡ 实时电流</text>
							<text class="card-value" :style="{ color: current > 10 ? '#e53935' : '#333' }">{{ current }} <text class="unit">A</text></text>
						</view>
					</view>
					
					<view class="control-panel">
					    <view class="status-header">
					        <text class="panel-title" style="border:none; margin:0; padding:0;">🎛️ 控制中枢</text>
					        <text class="mode-tag" :class="system_mode === 0 ? 'auto' : 'manual'">
					            {{ system_mode === 0 ? '🤖 自动 (AI托管)' : '🖐️ 手动 (人工接管)' }}
					        </text>
					    </view>
					    
					    <view class="pump-status-box">
					        <text class="status-label">水泵物理状态：</text>
					        <text class="status-text" :class="pump_status ? 'pump-on' : 'pump-off'">
					            {{ pump_status ? '💦 狂喷中' : '🛑 已停机' }}
					        </text>
					    </view>
					
						<view class="btn-group">
							<button class="force-btn" :class="pump_status ? 'btn-danger' : 'btn-primary'" @click="sendPumpCommand">
								强制 {{ pump_status ? '关闭' : '开启' }} 水泵
							</button>
							
							<button class="restore-btn" v-if="system_mode === 1" @click="restoreAutoMode">
								🔄 恢复 AI 自动托管
							</button>
						</view>
					</view>
					
					<view class="status-panel">
						<text class="panel-title">物理传感器报警</text>
						<view class="light-group">
							<view class="light-item">
								<view class="led" :class="smokeDetected ? 'led-red' : 'led-green'"></view>
								<text>烟雾超标</text>
							</view>
							<view class="light-item">
								<view class="led" :class="flameDo === 1 ? 'led-red' : 'led-green'"></view>
								<text>红外明火</text>
							</view>
						</view>
					</view>
				</scroll-view>
			</swiper-item>

		</swiper>
	</view>
</template>

<script>
export default {
	data() {
		return {
			currentTab: 0,
			
			// === 综合数据 ===
			riskLevel: 0,
			confidence: "0.0",
			lastUpdateTime: "正在连接云端...",
			timer: null,
			
			// === 视觉数据 ===
			imageConfidence: "0.0",
			fireArea: 0,
			fireCenterX: 0,
			fireCenterY: 0,
			imageFireDetected: false, 
			flickerDetected: false,   
			
			// === 传感器数据 ===
			smokePpm: "0.00",
			smokeDetected: false,
			temperature: "0.0",
			humidity: "0.0",          
			current: "0.00",          
			
			// === 控制状态 ===
			system_mode: 0,           // 0:自动, 1:手动
			pump_status: false,       // false:关, true:开
						
			flameAdc: 0,
			flameDo: 0,
			esp32FireDetected: false  
		}
	},
	onLoad() {
		this.fetchDataFromOneNet();
		this.timer = setInterval(() => {
			this.fetchDataFromOneNet();
		}, 3000); 
	},
	onUnload() {
		if (this.timer) clearInterval(this.timer);
	},
	methods: {
		switchTab(index) {
			this.currentTab = index;
		},
		onSwiperChange(e) {
			this.currentTab = e.detail.current;
		},
		
		// 1. 上行链路：从云端拉取数据
		fetchDataFromOneNet() {
			const productId = "PGs73D47Zf";
			const deviceName = "stm32f407zgt6";
			const token = "version=2018-10-31&res=products%2FPGs73D47Zf%2Fdevices%2Fstm32f407zgt6&et=1805469231&method=sha1&sign=snpTD9lxQKVsom7omCk5XnSUCG8%3D";

			uni.request({
				url: `https://iot-api.heclouds.com/thingmodel/query-device-property?product_id=${productId}&device_name=${deviceName}`,
				method: 'GET',
				header: { 'Authorization': token },
				success: (res) => {
					if (res.data.code === 0 && res.data.data) {
						let now = new Date();
						this.lastUpdateTime = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`;
						
						res.data.data.forEach(item => {
							const val = item.value;
							switch(item.identifier) {
								case 'risk_level': this.riskLevel = val; break;
								case 'confidence': this.confidence = val; break;
								case 'image_confidence': this.imageConfidence = val; break;
								case 'fire_area': this.fireArea = val; break;
								case 'fire_center_x': this.fireCenterX = val; break;
								case 'fire_center_y': this.fireCenterY = val; break;
								case 'image_fire_detected': this.imageFireDetected = val; break;
								case 'flicker_detected': this.flickerDetected = val; break;
								case 'smoke_ppm': this.smokePpm = val; break;
								case 'smoke_detected': this.smokeDetected = val; break;
								case 'temperature': this.temperature = val; break;
								case 'flame_adc': this.flameAdc = val; break;
								case 'flame_do': this.flameDo = val; break;
								case 'fire_detected': this.esp32FireDetected = val; break;
								
								// 强转类型，防止 UI 渲染出错
								case 'humidity': this.humidity = Number(val).toFixed(1); break;
								case 'current': this.current = Number(val).toFixed(2); break;
								case 'system_mode': this.system_mode = Number(val); break;
								case 'pump_status': 
									this.pump_status = (val === true || val === 'true' || val === 1 || val === '1'); 
									break;
							}
						});
					}
				}
			});
		},
		
		// 2. 下行链路核心代码：发送任意控制指令给云端
		sendControlCommand(payloadParams, loadingMsg, successMsg) {
			uni.showLoading({ title: loadingMsg });
			
			const productId = "PGs73D47Zf";
			const deviceName = "stm32f407zgt6";
			const token = "version=2018-10-31&res=products%2FPGs73D47Zf%2Fdevices%2Fstm32f407zgt6&et=1805469231&method=sha1&sign=snpTD9lxQKVsom7omCk5XnSUCG8%3D";

			uni.request({
				url: 'https://iot-api.heclouds.com/thingmodel/set-device-property',
				method: 'POST',
				header: { 
					'Authorization': token,
					'Content-Type': 'application/json' 
				},
				data: {
					product_id: productId,
					device_name: deviceName,
					params: payloadParams // 动态注入指令内容
				},
				success: (response) => {
					uni.hideLoading();
					if (response.data && response.data.code === 0) {
						uni.showToast({ title: successMsg, icon: 'success' });
					} else {
						uni.showToast({ title: '下发失败', icon: 'none' });
						console.error("OneNET 报错:", response.data);
					}
				},
				fail: (err) => {
					uni.hideLoading();
					uni.showToast({ title: '网络请求失败', icon: 'none' });
				}
			});
		},

		// 动作 A：强制开启/关闭水泵 (不再拦截自动模式！)
		sendPumpCommand() {
			uni.showModal({
				title: '紧急介入确认',
				content: `确定要强制 ${this.pump_status ? '关闭' : '开启'} 水泵吗？系统将切入人工接管模式！`,
				success: (res) => {
					if (res.confirm) {
						let targetState = !this.pump_status; 
						// 调用封装好的发送函数，打包 pump_status
						this.sendControlCommand(
							{ "pump_status": targetState }, 
							'强切水泵中...', 
							'指令已送达硬件'
						);
					}
				}
			});
		},
		
		// 动作 B：恢复 AI 自动托管 (下发 system_mode: 0)
		restoreAutoMode() {
			uni.showModal({
				title: '权限交还确认',
				content: '确定要将控制权交还给 AI 自动托管吗？水泵状态将由火情识别算法重新接管。',
				success: (res) => {
					if (res.confirm) {
						// 调用封装好的发送函数，打包 system_mode
						this.sendControlCommand(
							{ "system_mode": 0 }, 
							'切换模式中...', 
							'AI 已成功接管'
						);
					}
				}
			});
		}
	}
}
</script>

<style>
/* ... (基础样式保留不变) ... */
page { background-color: #f0f2f5; height: 100%; }
.container { display: flex; flex-direction: column; height: 100vh; }
.header { padding: 50rpx 40rpx 30rpx; color: #fff; transition: background-color 0.4s; }
.safe-bg { background: linear-gradient(135deg, #2b9939, #1e7b2a); }
.danger-bg { background: linear-gradient(135deg, #e53935, #c62828); animation: pulse 1.5s infinite; }
@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.85; } 100% { opacity: 1; } }
.title-box { display: flex; align-items: center; margin-bottom: 10rpx; }
.title-icon { font-size: 44rpx; margin-right: 10rpx; }
.title-text { font-size: 40rpx; font-weight: bold; }
.time-text { font-size: 24rpx; opacity: 0.8; }
.tab-bar { display: flex; background-color: #fff; padding: 0 20rpx; box-shadow: 0 4rpx 10rpx rgba(0,0,0,0.05); z-index: 10; }
.tab-item { flex: 1; text-align: center; padding: 24rpx 0; font-size: 30rpx; color: #666; border-bottom: 6rpx solid transparent; transition: all 0.3s; }
.tab-item.active { color: #2b9939; font-weight: bold; border-bottom-color: #2b9939; }
.swiper-box { flex: 1; }
.scroll-page { height: 100%; padding: 20rpx 0; }
.card-container { display: flex; flex-wrap: wrap; justify-content: space-between; padding: 0 30rpx; }
.data-card { width: 45%; background-color: #fff; border-radius: 16rpx; padding: 24rpx; margin-bottom: 24rpx; box-sizing: border-box; box-shadow: 0 4rpx 12rpx rgba(0,0,0,0.03); }
.main-card { width: 100%; display: flex; flex-direction: column; align-items: center;}
.clickable { border: 2rpx dashed #2b9939; }
.clickable:active { opacity: 0.7; }
.card-title { font-size: 26rpx; color: #777; margin-bottom: 16rpx; display: block; }
.card-value { font-size: 42rpx; font-weight: bold; color: #333; }
.unit { font-size: 22rpx; color: #aaa; font-weight: normal; margin-left: 4rpx;}
.card-desc { font-size: 24rpx; margin-top: 10rpx; color: #666; }
.safe-text .card-value { color: #2b9939; }
.danger-text .card-value { color: #e53935; }
.status-panel { margin: 10rpx 30rpx 40rpx; background-color: #fff; border-radius: 16rpx; padding: 24rpx; box-shadow: 0 4rpx 12rpx rgba(0,0,0,0.03); }
.panel-title { font-size: 26rpx; color: #777; border-bottom: 1px solid #eee; padding-bottom: 16rpx; display: block; margin-bottom: 20rpx; }
.light-group { display: flex; justify-content: space-around; }
.light-item { display: flex; flex-direction: column; align-items: center; font-size: 24rpx; color: #555; }
.led { width: 40rpx; height: 40rpx; border-radius: 50%; margin-bottom: 12rpx; box-shadow: inset 0 2rpx 6rpx rgba(0,0,0,0.3), 0 0 10rpx rgba(0,0,0,0.1); }
.led-green { background: radial-gradient(circle at 30% 30%, #4caf50, #1b5e20); box-shadow: 0 0 15rpx #4caf50; }
.led-red { background: radial-gradient(circle at 30% 30%, #f44336, #b71c1c); box-shadow: 0 0 20rpx #f44336; animation: blink 0.8s infinite; }

/* ================= 控制中枢专属样式 ================= */
.control-panel {
    margin: 10rpx 30rpx 24rpx;
    background-color: #fff;
    border-radius: 16rpx;
    padding: 30rpx 24rpx;
    box-shadow: 0 4rpx 12rpx rgba(0,0,0,0.05);
    border-left: 8rpx solid #1890ff; 
}
.status-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
    border-bottom: 1px solid #eee;
    padding-bottom: 20rpx;
    margin-bottom: 20rpx;
}
.mode-tag {
    padding: 6rpx 16rpx;
    border-radius: 20rpx;
    font-size: 22rpx;
    font-weight: bold;
}
.mode-tag.auto { background-color: #e6f7ff; color: #1890ff; border: 1px solid #91d5ff; }
.mode-tag.manual { background-color: #fff2e8; color: #fa541c; border: 1px solid #ffbb96; }

.pump-status-box {
    display: flex;
    align-items: center;
    margin-bottom: 30rpx;
    font-size: 28rpx;
}
.status-label { color: #666; }
.status-text { font-weight: bold; font-size: 32rpx; margin-left: 10rpx; }
.pump-on { color: #1890ff; } 
.pump-off { color: #999; }   

/* 👇 按键组与新增按钮样式 👇 */
.btn-group {
    display: flex;
    flex-direction: column;
    gap: 20rpx; /* 两个按钮之间的间距 */
}
.force-btn {
    width: 100%;
    border-radius: 40rpx;
    font-size: 30rpx;
    font-weight: bold;
    letter-spacing: 2rpx;
}
.btn-primary { background-color: #2b9939 !important; color: #fff !important; }
.btn-danger { background-color: #e53935 !important; color: #fff !important; }

.restore-btn {
    width: 100%;
    border-radius: 40rpx;
    font-size: 28rpx;
    font-weight: bold;
    color: #1890ff !important;
    background-color: #e6f7ff !important;
    border: 1px solid #91d5ff !important;
}
.restore-btn:active { background-color: #bae0ff !important; }

</style>
