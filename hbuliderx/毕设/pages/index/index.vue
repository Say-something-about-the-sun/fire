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
					
					    <button class="force-btn" :class="pump_status ? 'btn-danger' : 'btn-primary'" @click="sendPumpCommand">
					        强制 {{ pump_status ? '关闭' : '开启' }} 水泵
					    </button>
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
			humidity: "0.0",          // 新增：湿度
			current: "0.00",          // 新增：电流
			
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
		}, 3000); // 建议刷新频率调快到 3 秒，体验更好
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
		
		fetchDataFromOneNet() {
			const productId = "PGs73D47Zf";
			const deviceName = "stm32f407zgt6";
			// 注意：实际项目中 Token 过期需要重新生成，测试阶段暂用你的静态 Token
			const token = "version=2018-10-31&res=products%2FPGs73D47Zf%2Fdevices%2Fstm32f407zgt6&et=1805469231&method=sha1&sign=snpTD9lxQKVsom7omCk5XnSUCG8%3D";

			uni.request({
				url: `https://iot-api.heclouds.com/thingmodel/query-device-property?product_id=${productId}&device_name=${deviceName}`,
				method: 'GET',
				header: { 'Authorization': token },
				success: (res) => {
					if (res.data.code === 0 && res.data.data) {
						let now = new Date();
						this.lastUpdateTime = `${now.getHours().toString().padStart(2, '0')}:${now.getMinutes().toString().padStart(2, '0')}:${now.getSeconds().toString().padStart(2, '0')}`;
						
						// 解析所有返回的数据
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
								
								// 👇 本次补全的新增物模型解析 👇
								case 'humidity': this.humidity = val; break;
								case 'current': this.current = val; break;
								case 'system_mode': this.system_mode = val; break;
								case 'pump_status': this.pump_status = val; break;
							}
						});
					}
				}
			});
		},
		
		// 强制开关水泵按钮的触发函数
		sendPumpCommand() {
			// 交互优化：如果 STM32 处于 AI 自动托管模式，提醒用户先切换模式
			if (this.system_mode === 0) {
				uni.showToast({
					title: 'AI托管中，请先在开发板按下 WK_UP 切换为手动模式',
					icon: 'none',
					duration: 3000
				});
				return;
			}
			
			// 弹出安全确认框
			uni.showModal({
				title: '控制指令下发',
				content: `确定要强制 ${this.pump_status ? '关闭' : '开启'} 水泵吗？`,
				success: (res) => {
					if (res.confirm) {
						let targetState = this.pump_status ? 0 : 1; // 取反
						
						// 模拟成功效果 (下一步我们将在这里写实际的 API 请求代码)
						uni.showToast({ title: '指令已下发云端', icon: 'success' });
						console.log(`准备向 OneNET 发送指令 -> pump_cmd: ${targetState}`);
					}
				}
			});
		}
	}
}
</script>

<style>
/* 基础样式保持不变 */
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

/* 👇 本次新增的 控制中枢 CSS 👇 */
.control-panel {
    margin: 10rpx 30rpx 24rpx;
    background-color: #fff;
    border-radius: 16rpx;
    padding: 30rpx 24rpx;
    box-shadow: 0 4rpx 12rpx rgba(0,0,0,0.05);
    border-left: 8rpx solid #1890ff; /* 左侧科技蓝点缀 */
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
.pump-on { color: #1890ff; } /* 喷水时蓝色 */
.pump-off { color: #999; }   /* 停机时灰色 */

.force-btn {
    width: 100%;
    border-radius: 40rpx;
    font-size: 30rpx;
    font-weight: bold;
    letter-spacing: 2rpx;
}
/* 动态按钮颜色：水泵关着时是绿色(开机)，开着时是红色(强制停机) */
.btn-primary { background-color: #2b9939 !important; color: #fff !important; }
.btn-danger { background-color: #e53935 !important; color: #fff !important; }

</style>
