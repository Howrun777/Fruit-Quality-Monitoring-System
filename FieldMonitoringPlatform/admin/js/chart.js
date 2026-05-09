let fruitHistoryChart = null;
let tempChart = null;
let humChart = null;
let lightChart = null;
let spoilChart = null;

/**
 * 横向滚轮接管函数
 * 当鼠标悬浮在横向容器上时，将垂直滚轮事件转换为水平滚动
 */
function setupHorizontalScroll(containerId) {
    const container = document.getElementById(containerId);
    if (!container) return;
    
    container.addEventListener('wheel', (e) => {
        // 只有当容器有横向滚动内容时才接管
        if (container.scrollWidth > container.clientWidth + 5) {
            e.preventDefault();
            // 将垂直滚轮 deltaY 转换为水平滚动
            container.scrollLeft += e.deltaY;
        }
    }, { passive: false });
}

/**
 * AI视觉快照画廊渲染函数
 * @param {Array} dataList - 图片数据数组，格式: [{imgUrl, quality, time}, ...]
 * 渲染完成后自动滚动到最右侧，显示最新图片
 */
function renderVisionGallery(dataList) {
    const container = document.getElementById('vision-gallery-container');
    if (!container) return;
    
    container.innerHTML = '';
    
    if (!dataList || dataList.length === 0) {
        container.innerHTML = '<div style="padding: 30px 20px; color: #999; text-align: center; width: 100%;">暂无快照数据</div>';
        return;
    }
    
    dataList.forEach(item => {
        const div = document.createElement('div');
        div.className = 'vision-item';
        div.innerHTML = `
            <img src="${item.imgUrl}" alt="樱桃快照" onerror="this.style.display='none';">
            <span class="quality-tag">品质: ${item.quality}</span>
            <span class="time-tag">${item.time}</span>
        `;
        container.appendChild(div);
    });
    
    // 渲染完成后，延迟滚动到最右侧（显示最新图片）
    requestAnimationFrame(() => {
        container.scrollLeft = container.scrollWidth;
    });
}

/**
 * 测试假数据 - 用于后端接口未完成时的调试
 * 使用示例: renderVisionGallery(getMockVisionData());
 */
function getMockVisionData() {
    // 使用服务器绝对路径，避免 file:// 协议限制
    const serverIP = "http://47.107.41.102:9000";
    const imgPath = `${serverIP}/assets/uploads/1001-01-01_1712800000.jpg`;

    return [
        { imgUrl: imgPath, quality: 10, time: '4.29-23:15' },
        { imgUrl: imgPath, quality: 9, time: '4.29-23:10' },
        { imgUrl: imgPath, quality: 8, time: '4.29-23:05' },
        { imgUrl: imgPath, quality: 7, time: '4.29-23:00' },
        { imgUrl: imgPath, quality: 9, time: '4.29-22:55' },
        { imgUrl: imgPath, quality: 6, time: '4.29-22:50' },
        { imgUrl: imgPath, quality: 8, time: '4.29-22:45' },
    ];
}

document.addEventListener('DOMContentLoaded', () => {
    fruitHistoryChart = echarts.init(document.getElementById('fruitHistoryChart'));
    tempChart = echarts.init(document.getElementById('tempChart'));
    humChart = echarts.init(document.getElementById('humChart'));
    lightChart = echarts.init(document.getElementById('lightChart'));
    spoilChart = echarts.init(document.getElementById('spoilChart'));

    // 初始化横向滚轮接管
    setupHorizontalScroll('fruit-grid-container');
    setupHorizontalScroll('vision-gallery-container');

    // 初始化视觉数据画廊为空（等待 real 数据加载）
    renderVisionGallery([]);

    window.addEventListener('resize', () => {
        fruitHistoryChart.resize();
        tempChart.resize(); 
        humChart.resize(); 
        lightChart.resize();
        spoilChart.resize(); 
    });
});

function buildStrictOption(data, min, max, interval, colorsArray, timeRange, threshold = null, yAxisSuffix = '') {
    let option = {
        grid: { left: '10%', right: '4%', bottom: '15%', top: '10%' },
        tooltip: { 
            trigger: 'axis',
            axisPointer: { type: 'line' },
            formatter: function (params) {
                if (!params || params.length === 0) return '';
                let date = new Date(params[0].value[0]);
                let Y = date.getFullYear();
                let M = date.getMonth() + 1;
                let d = date.getDate();
                let h = date.getHours().toString().padStart(2, '0');
                let m = date.getMinutes().toString().padStart(2, '0');
                let s = date.getSeconds().toString().padStart(2, '0');
                
                let timeStr = `${Y}年${M}月${d}日 ${h}:${m}:${s}`;
                let html = `<div style="font-size:0.9rem; color:#666; margin-bottom:5px;">${timeStr}</div>`;
                
                params.forEach(param => {
                    let val = param.value[1];
                    if (typeof val === 'number' && !Number.isInteger(val)) val = val.toFixed(1);
                    html += `<div>${param.marker} ${param.seriesName}: <span style="font-weight:bold; color:#333;">${val}${yAxisSuffix}</span></div>`;
                });
                return html;
            }
        },
        xAxis: { 
            type: 'value', 
            min: timeRange.min, 
            max: timeRange.max, 
            interval: timeRange.interval, 
            axisTick: { show: false }, 
            axisLine: { show: false },
            axisLabel: {
                showMinLabel: true, 
                showMaxLabel: true, 
                formatter: function (value) {
                    let date = new Date(value);
                    let M = (date.getMonth() + 1).toString().padStart(2, '0');
                    let d = date.getDate().toString().padStart(2, '0');
                    let h = date.getHours().toString().padStart(2, '0');
                    let m = date.getMinutes().toString().padStart(2, '0');
                    
                    if (timeRange.interval >= 24 * 3600 * 1000) return `${M}.${d}`; 
                    else return `${h}:${m}`; 
                }
            }
        },
        yAxis: { 
            type: 'value', 
            min: min, max: max, interval: interval,
            axisLabel: {
                formatter: `{value}${yAxisSuffix}`,
                color: (value) => {
                    let idx = Math.round((value - min) / interval);
                    return colorsArray[idx] || '#333';
                }
            },
            splitLine: { show: true, lineStyle: { type: 'solid', color: colorsArray } }
        },
        series: [{
            name: '数值',
            type: 'line',
            data: data, 
            smooth: false,
            showSymbol: true,          
            symbol: 'circle',          
            symbolSize: 6,             
            itemStyle: { color: '#D93843' }, 
            lineStyle: { color: '#8B1820', width: 2.5 }, 
            connectNulls: false
        }]
    };

    if (threshold !== null) {
        option.series[0].markLine = {
            symbol: 'none',
            data: [{ yAxis: threshold }],
            lineStyle: { color: '#f39c12', type: 'solid', width: 2 },
            label: { show: false }
        };
    }
    return option;
}

function renderFruitHistoryChart(data, deviceId, timeRange, maturityThreshold = 15.0) {
    document.getElementById('fruit-history-title').innerText = `[${deviceId}] 糖度历史变化`;
    const colors = ['#333', '#333', '#333', '#333', '#333', '#333', '#D93843', '#D93843', '#D93843'];
    fruitHistoryChart.setOption(buildStrictOption(data, 0, 25, 3, colors, timeRange, maturityThreshold), true);
}

function renderSpoilHistoryChart(data, timeRange) {
    const spoilColors = ['#333', '#333', '#333', '#e74c3c', '#e74c3c']; 
    let option = buildStrictOption(data, 0, 100, 20, spoilColors, timeRange, 60, '%');
    option.series[0].name = "腐败度";
    option.series[0].areaStyle = {
        color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(217, 56, 67, 0.5)' },
            { offset: 1, color: 'rgba(217, 56, 67, 0.05)' }
        ])
    };
    spoilChart.setOption(option, true);
}

function renderEnvHistoryCharts(tempData, humData, lightData, tempRange, humRange, lightRange) {
    const tempColors = ['#D93843', '#D93843', '#333', '#333', '#333', '#D93843', '#D93843', '#D93843'];
    tempChart.setOption(buildStrictOption(tempData, -10, 60, 10, tempColors, tempRange), true);

    const humColors = ['#D93843', '#D93843', '#333', '#333', '#333', '#D93843', '#D93843'];
    humChart.setOption(buildStrictOption(humData, 30, 90, 10, humColors, humRange), true);

    const lightColors = ['#D93843', '#D93843', '#D93843', '#333', '#333', '#333', '#333', '#D93843', '#D93843'];
    lightChart.setOption(buildStrictOption(lightData, 0, 90000, 10000, lightColors, lightRange), true);
}

function renderFruitGrid(fruitList) {
    const container = document.getElementById('fruit-grid-container');
    container.innerHTML = '';
    
    let ripeCount = 0;
    
    if(fruitList.length === 0) {
        document.getElementById('fruit-summary').innerText = `总计: 0 | 已熟: 0`;
        return;
    }

    const currentUnixTime = Math.floor(new Date().getTime() / 1000);
    const OFFLINE_THRESHOLD = 30 * 60; 

    fruitList.forEach(fruit => {
        let matPercent = Math.round(fruit.maturity_score * 100);
        const isRipe = fruit.maturity_score >= 1.0;
        if (isRipe) ripeCount++;
        
        let isOffline = false;
        if (fruit.collected_at) {
            isOffline = (currentUnixTime - fruit.collected_at) > OFFLINE_THRESHOLD;
        }
        
        const card = document.createElement('div');
        card.className = `fruit-item ${isOffline ? 'offline' : ''}`;
        
        let htmlContent = `
            <div class="fruit-id">${fruit.device_id}</div>
            <div class="fruit-status ${isRipe ? 'status-ripe' : 'status-unripe'}">${isRipe ? '成熟' : '生长'}(${matPercent}%)</div>
            <div class="fruit-data">${fruit.sugar_brix.toFixed(1)} <small>Brix</small></div>
        `;
        
        if (isOffline) {
            htmlContent += `<span class="fruit-offline-alert">⚠️ 超时未更新</span>`;
        }
        
        card.innerHTML = htmlContent;

        card.onclick = () => {
            document.querySelectorAll('.fruit-item').forEach(el => el.classList.remove('active'));
            card.classList.add('active');
            window.currentSelectedFruitId = fruit.device_id;
            if(window.reloadAllChartsData) window.reloadAllChartsData();
            if(window.resetIdleTimer) window.resetIdleTimer();
        };
        container.appendChild(card);
    });
    
    document.getElementById('fruit-summary').innerText = `总计: ${fruitList.length} | 已熟: ${ripeCount}`;
}