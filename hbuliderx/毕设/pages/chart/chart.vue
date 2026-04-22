<template>
	<view class="chart-container">
		<view class="chart-header">
			<text class="chart-title">{{ chartTitle }} 历史趋势</text>
			<text class="chart-subtitle">最近 20 次采样点</text>
		</view>
		
		<view class="charts-box">
			<qiun-data-charts 
				type="line"
				:opts="opts"
				:chartData="chartData"
				:ontouch="true"
			/>
		</view>

		<view class="data-analysis" v-if="historyList.length > 0">
			<view class="analysis-item">
				<text class="label">最高值</text>
				<text class="value">{{ maxValue }}</text>
			</view>
			<view class="analysis-item">
				<text class="label">平均值</text>
				<text class="value">{{ avgValue }}</text>
			</view>
		</view>
	</view>
</template>

<script>
export default {
	data() {
		return {
			type: '', // temperature 或 smoke_ppm
			chartTitle: '',
			chartData: {},
			historyList: [],
			// 圖表配置項目
			opts: {
				color: ["#1890FF"],
				padding: [15, 10, 0, 15],
				enableScroll: true,
				xAxis: {
					disableGrid: true,
					scrollShow: true,
					itemCount: 5, // 視窗內顯示 5 個點
				},
				yAxis: {
					gridType: "dash",
					dashLength: 2
				},
				extra: {
					line: {
						type: "curve", // 曲線模式更流暢
						width: 2
					}
				}
			}
		};
	},
	onLoad(options) {
		this.type = options.type || 'temperature';
		this.chartTitle = this.type === 'temperature' ? '环境温度' : '烟雾浓度';
		this.getHistoryData();
	},
	computed: {
		maxValue() {
			return Math.max(...this.historyList.map(item => item.value)).toFixed(1);
		},
		avgValue() {
			let sum = this.historyList.reduce((acc, cur) => acc + cur.value, 0);
			return (sum / this.historyList.length).toFixed(1);
		}
	},
	methods: {
		// 🚨 這裡需要調用 OneNET 的歷史數據查詢接口 (datapoints)
		// 🚨 真实云端历史数据拉取
				getHistoryData() {
					const productId = "PGs73D47Zf";
					const deviceName = "stm32f407zgt6";
					const token = "version=2018-10-31&res=products%2FPGs73D47Zf%2Fdevices%2Fstm32f407zgt6&et=1805469231&method=sha1&sign=snpTD9lxQKVsom7omCk5XnSUCG8%3D";
		
					// 计算时间范围：拉取过去 1 小时内的数据
					const endTime = Date.now();
					const startTime = endTime - 60 * 60 * 1000; 
		
					// 匹配 OneNET 平台上的标识符
					let identifier = this.type === 'temperature' ? 'temperature' : 'smoke_ppm';
		
					uni.showLoading({ title: '加载历史数据...' });
		
					uni.request({
						url: 'https://iot-api.heclouds.com/thingmodel/query-device-property-history',
						method: 'GET',
						data: {
							product_id: productId,
							device_name: deviceName,
							identifier: identifier,
							start_time: startTime,
							end_time: endTime,
							limit: 20 // 限制 20 个点，防止手机端图表过于拥挤卡顿
						},
						header: { 'Authorization': token },
						success: (res) => {
							uni.hideLoading();
							if (res.data.code === 0 && res.data.data && res.data.data.list) {
								
								// 🚨 核心逻辑：OneNET 返回的数据是倒序的（最新的在最前）
								// 折线图需要正序（最新的在最右边），所以必须 reverse()
								let apiData = res.data.data.list.reverse();
		
								let formatData = apiData.map(item => {
									let date = new Date(item.time);
									let timeStr = `${date.getHours().toString().padStart(2, '0')}:${date.getMinutes().toString().padStart(2, '0')}`;
									return { time: timeStr, value: parseFloat(item.value) };
								});
		
								this.historyList = formatData;
								
								// 组装图表所需的 JSON 结构
								let chartRes = {
									categories: formatData.map(item => item.time),
									series: [{
										name: this.chartTitle,
										data: formatData.map(item => item.value)
									}]
								};
								this.chartData = JSON.parse(JSON.stringify(chartRes));
							} else {
								uni.showToast({ title: '云端暂无历史数据', icon: 'none' });
							}
						},
						fail: () => {
							uni.hideLoading();
							uni.showToast({ title: '网络请求超时', icon: 'none' });
						}
					});
				}
	}
}
</script>

<style scoped>
.chart-container { padding: 30rpx; background-color: #fff; height: 100vh; }
.chart-header { margin-bottom: 40rpx; }
.chart-title { font-size: 36rpx; font-weight: bold; color: #333; display: block; }
.chart-subtitle { font-size: 24rpx; color: #999; }
.charts-box { width: 100%; height: 500rpx; }
.data-analysis { display: flex; justify-content: space-around; margin-top: 50rpx; padding: 30rpx; background-color: #f8f9fa; border-radius: 20rpx; }
.analysis-item { display: flex; flex-direction: column; align-items: center; }
.label { font-size: 24rpx; color: #666; margin-bottom: 10rpx; }
.value { font-size: 32rpx; font-weight: bold; color: #1890ff; }
</style>
