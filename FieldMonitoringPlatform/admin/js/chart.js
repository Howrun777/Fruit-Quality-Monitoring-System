let fruitHistoryChart = null;
let tempChart = null;
let humChart = null;
let lightChart = null;
let spoilChart = null;

const OFFLINE_THRESHOLD_SECONDS = 30 * 60;
const chartTheme = {
    brand: '#b4232d',
    brandSoft: 'rgba(180, 35, 45, 0.12)',
    green: '#168a5a',
    amber: '#c87800',
    red: '#d92d20',
    blue: '#2563eb',
    ink: '#1f2937',
    muted: '#667085',
    grid: '#e6eaf0'
};

function hexToRgba(hex, alpha) {
    const normalized = hex.replace('#', '');
    const value = parseInt(normalized.length === 3
        ? normalized.split('').map(ch => ch + ch).join('')
        : normalized, 16);
    const red = (value >> 16) & 255;
    const green = (value >> 8) & 255;
    const blue = value & 255;
    return `rgba(${red}, ${green}, ${blue}, ${alpha})`;
}

function setupHorizontalScroll(containerId) {
    const container = document.getElementById(containerId);
    if (!container) return;

    container.addEventListener('wheel', (event) => {
        if (container.scrollWidth > container.clientWidth + 5) {
            event.preventDefault();
            container.scrollLeft += event.deltaY;
        }
    }, { passive: false });
}

function initCharts() {
    fruitHistoryChart = echarts.init(document.getElementById('fruitHistoryChart'));
    tempChart = echarts.init(document.getElementById('tempChart'));
    humChart = echarts.init(document.getElementById('humChart'));
    lightChart = echarts.init(document.getElementById('lightChart'));
    spoilChart = echarts.init(document.getElementById('spoilChart'));
}

function resizeCharts() {
    [fruitHistoryChart, tempChart, humChart, lightChart, spoilChart].forEach(chart => {
        if (chart) chart.resize();
    });
}

function formatDateTime(ms, mode = 'full') {
    const date = new Date(ms);
    const month = String(date.getMonth() + 1).padStart(2, '0');
    const day = String(date.getDate()).padStart(2, '0');
    const hour = String(date.getHours()).padStart(2, '0');
    const minute = String(date.getMinutes()).padStart(2, '0');
    const second = String(date.getSeconds()).padStart(2, '0');
    if (mode === 'short') return `${month}.${day} ${hour}:${minute}`;
    return `${date.getFullYear()}-${month}-${day} ${hour}:${minute}:${second}`;
}

function statusTextByMetric(metric, value) {
    if (value === null || value === undefined || Number.isNaN(value)) return '无数据';
    if (metric === 'temp') {
        if (value < 10) return '低温异常';
        if (value > 40) return '高温异常';
        return '温度稳定';
    }
    if (metric === 'hum') {
        if (value < 40) return '湿度偏低';
        if (value > 80) return '湿度偏高';
        return '湿度适宜';
    }
    if (metric === 'light') {
        if (value < 1) return '光照偏低';
        if (value > 7) return '强光风险';
        return '光照正常';
    }
    if (metric === 'spoil') {
        if (value > 60) return '腐败预警';
        if (value > 35) return '风险升高';
        return '风险可控';
    }
    if (metric === 'sugar') {
        if (value >= 15) return '达到成熟糖度';
        return '糖度累积中';
    }
    return '正常';
}

function buildOption({
    name,
    data,
    timeRange,
    yMin,
    yMax,
    yInterval,
    unit = '',
    color = chartTheme.brand,
    threshold = null,
    thresholdName = '阈值',
    thresholdLines = null,
    yFormatter = null,
    metric = ''
}) {
    const hasData = Array.isArray(data) && data.length > 0;
    const markLines = Array.isArray(thresholdLines)
        ? thresholdLines
        : (threshold === null ? [] : [{ value: threshold, name: thresholdName, color: chartTheme.amber }]);

    return {
        color: [color],
        grid: { left: 60, right: 42, bottom: 34, top: 26 },
        tooltip: {
            trigger: 'axis',
            backgroundColor: 'rgba(255,255,255,0.96)',
            borderColor: '#e6eaf0',
            borderWidth: 1,
            padding: [10, 12],
            textStyle: { color: chartTheme.ink, fontSize: 12 },
            axisPointer: {
                type: 'line',
                lineStyle: { color: '#98a2b3', type: 'dashed' }
            },
            formatter(params) {
                if (!params || !params.length) return '';
                const point = params[0];
                const value = point.value[1];
                const formatted = typeof value === 'number'
                    ? (metric === 'light' ? Number(value.toFixed(2)).toString() : value.toFixed(1))
                    : value;
                return `
                    <div style="font-weight:800;margin-bottom:6px;">${formatDateTime(point.value[0])}</div>
                    <div>${point.marker}${name}: <strong>${formatted}${unit}</strong></div>
                    <div style="margin-top:4px;color:#667085;">${statusTextByMetric(metric, value)}</div>
                `;
            }
        },
        xAxis: {
            type: 'value',
            min: timeRange.min,
            max: timeRange.max,
            interval: timeRange.interval,
            axisLine: { show: false },
            axisTick: { show: false },
            splitLine: { show: false },
            axisLabel: {
                color: chartTheme.muted,
                fontSize: 11,
                hideOverlap: true,
                formatter(value) {
                    const date = new Date(value);
                    const month = String(date.getMonth() + 1).padStart(2, '0');
                    const day = String(date.getDate()).padStart(2, '0');
                    const hour = String(date.getHours()).padStart(2, '0');
                    const minute = String(date.getMinutes()).padStart(2, '0');
                    return timeRange.interval >= 24 * 3600 * 1000 ? `${month}.${day}` : `${hour}:${minute}`;
                }
            }
        },
        yAxis: {
            type: 'value',
            min: yMin,
            max: yMax,
            interval: yInterval,
            axisLine: { show: false },
            axisTick: { show: false },
            splitLine: { lineStyle: { color: chartTheme.grid } },
            axisLabel: {
                color: chartTheme.muted,
                fontSize: 11,
                formatter(value) {
                    return typeof yFormatter === 'function' ? yFormatter(value) : `${value}${unit}`;
                }
            }
        },
        series: [{
            name,
            type: 'line',
            data,
            smooth: true,
            showSymbol: hasData,
            symbol: 'circle',
            symbolSize: 6,
            connectNulls: false,
            lineStyle: { width: 3, color },
            itemStyle: { color, borderColor: '#fff', borderWidth: 2 },
            areaStyle: {
                color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
                    { offset: 0, color: hexToRgba(color, 0.20) },
                    { offset: 1, color: 'rgba(255,255,255,0)' }
                ])
            },
            markLine: markLines.length === 0 ? undefined : {
                symbol: 'none',
                data: markLines.map(line => ({
                    yAxis: line.value,
                    name: line.name,
                    lineStyle: {
                        color: line.color || chartTheme.amber,
                        width: line.width || 2,
                        type: line.type || 'dashed'
                    },
                    label: {
                        color: line.color || chartTheme.amber,
                        fontWeight: 800,
                        position: line.position || 'insideEndTop',
                        distance: line.distance || [0, 8],
                        formatter: line.name
                    }
                }))
            }
        }]
    };
}

function renderFruitHistoryChart(data, deviceId, timeRange, maturityThreshold = 15.0) {
    const title = document.getElementById('fruit-history-title');
    if (title) title.innerText = deviceId ? `${deviceId} 糖度历史` : '请选择设备';
    if (!fruitHistoryChart) return;
    fruitHistoryChart.setOption(buildOption({
        name: '糖度',
        data,
        timeRange,
        yMin: 0,
        yMax: 25,
        yInterval: 5,
        unit: ' Brix',
        color: chartTheme.brand,
        threshold: maturityThreshold,
        thresholdName: '成熟糖度',
        metric: 'sugar'
    }), true);
}

function renderSpoilHistoryChart(data, timeRange) {
    if (!spoilChart) return;
    spoilChart.setOption(buildOption({
        name: '腐败度',
        data,
        timeRange,
        yMin: 0,
        yMax: 100,
        yInterval: 20,
        unit: '%',
        color: chartTheme.red,
        threshold: 60,
        thresholdName: '预警线',
        metric: 'spoil'
    }), true);
}

function renderEnvHistoryCharts(tempData, humData, lightData, tempRange, humRange, lightRange) {
    if (tempChart) {
        tempChart.setOption(buildOption({
            name: '温度',
            data: tempData,
            timeRange: tempRange,
            yMin: -10,
            yMax: 60,
            yInterval: 10,
            unit: '℃',
            color: chartTheme.red,
            thresholdLines: [
                { value: 10, name: '低温', color: chartTheme.blue, position: 'insideEndBottom', distance: [0, 8] },
                { value: 40, name: '高温', color: chartTheme.amber }
            ],
            metric: 'temp'
        }), true);
    }

    if (humChart) {
        humChart.setOption(buildOption({
            name: '湿度',
            data: humData,
            timeRange: humRange,
            yMin: 30,
            yMax: 90,
            yInterval: 10,
            unit: '%',
            color: chartTheme.blue,
            thresholdLines: [
                { value: 40, name: '低湿度', color: chartTheme.amber, position: 'insideEndBottom', distance: [0, 8] },
                { value: 80, name: '湿度上限', color: chartTheme.amber }
            ],
            metric: 'hum'
        }), true);
    }

    if (lightChart) {
        const lightInTenThousandLux = Array.isArray(lightData)
            ? lightData.map(point => [point[0], Number(point[1] || 0) / 10000])
            : [];
        lightChart.setOption(buildOption({
            name: '光照',
            data: lightInTenThousandLux,
            timeRange: lightRange,
            yMin: 0,
            yMax: 9,
            yInterval: 1.5,
            unit: ' 万Lux',
            color: chartTheme.amber,
            thresholdLines: [
                { value: 1, name: '低光照', color: chartTheme.blue, position: 'insideEndBottom', distance: [0, 8] },
                { value: 7, name: '强光', color: chartTheme.amber }
            ],
            yFormatter(value) {
                return `${Number(value).toFixed(value % 1 === 0 ? 0 : 1)}万Lux`;
            },
            metric: 'light'
        }), true);
    }
}

function renderFruitGrid(fruitList) {
    const container = document.getElementById('fruit-grid-container');
    if (!container) return;
    container.innerHTML = '';

    const now = Math.floor(Date.now() / 1000);
    const list = Array.isArray(fruitList) ? fruitList : [];
    let ripeCount = 0;
    let offlineCount = 0;

    if (!list.length) {
        container.innerHTML = '<div class="empty-state">当前产区暂无设备数据</div>';
        updateDashboardStats({ total: 0, online: 0, offline: 0, ripe: 0 });
        return;
    }

    list.forEach((fruit) => {
        const maturity = Number(fruit.maturity_score || 0);
        const brix = Number(fruit.sugar_brix || 0);
        const percent = Math.max(0, Math.round(maturity * 100));
        const isRipe = maturity >= 1.0;
        const isOffline = fruit.collected_at ? (now - Number(fruit.collected_at)) > OFFLINE_THRESHOLD_SECONDS : true;

        if (isRipe) ripeCount += 1;
        if (isOffline) offlineCount += 1;

        const card = document.createElement('button');
        card.type = 'button';
        card.className = `device-card ${isOffline ? 'offline' : ''}`;
        if (fruit.device_id === window.currentSelectedFruitId) card.classList.add('active');

        const statusClass = isOffline ? 'offline' : (isRipe ? 'good' : 'warn');
        const statusText = isOffline ? '离线' : (isRipe ? '成熟' : '生长期');

        card.innerHTML = `
            <div class="device-row">
                <span class="device-id">${fruit.device_id}</span>
                <span class="status-pill ${statusClass}">${statusText}</span>
            </div>
            <div class="device-meta">
                <span>糖度<strong>${brix.toFixed(1)}</strong></span>
                <span>成熟度<strong>${percent}%</strong></span>
            </div>
            <div class="progress-track">
                <div class="progress-bar" style="width:${Math.min(percent, 120)}%"></div>
            </div>
        `;

        card.addEventListener('click', () => {
            document.querySelectorAll('.device-card').forEach(el => el.classList.remove('active'));
            card.classList.add('active');
            window.currentSelectedFruitId = fruit.device_id;
            if (window.reloadAllChartsData) window.reloadAllChartsData();
            if (window.resetIdleTimer) window.resetIdleTimer();
        });

        container.appendChild(card);
    });

    updateDashboardStats({
        total: list.length,
        online: list.length - offlineCount,
        offline: offlineCount,
        ripe: ripeCount
    });
}

function updateDashboardStats({ total = 0, online = 0, offline = 0, ripe = 0, alerts = null }) {
    const summary = document.getElementById('fruit-summary');
    if (summary) summary.innerText = `总数 ${total} · 成熟 ${ripe}`;
    const onlineEl = document.getElementById('stat-online');
    const offlineEl = document.getElementById('stat-offline');
    const ripeEl = document.getElementById('stat-ripe');
    const alertsEl = document.getElementById('stat-alerts');
    if (onlineEl) onlineEl.innerText = online;
    if (offlineEl) offlineEl.innerText = offline;
    if (ripeEl) ripeEl.innerText = ripe;
    if (alertsEl && alerts !== null) alertsEl.innerText = alerts;
}

function renderVisionGallery(dataList) {
    const container = document.getElementById('vision-gallery-container');
    const count = document.getElementById('vision-count');
    if (!container) return;

    const list = Array.isArray(dataList) ? dataList : [];
    if (count) count.innerText = `${list.length} 张`;
    container.innerHTML = '';

    if (!list.length) {
        container.innerHTML = '<div class="empty-state">暂无视觉识别记录</div>';
        return;
    }

    list.forEach((item) => {
        const div = document.createElement('article');
        div.className = 'vision-item';
        const qualityError = item.quality === 'N/A' || item.quality === -1;
        const qualityText = qualityError ? '推理失败' : `等级 ${item.quality}`;
        div.innerHTML = `
            <div class="vision-image-wrap">
                <img src="${item.imgUrl}" alt="视觉识别图片" loading="lazy">
            </div>
            <div class="vision-info">
                <div class="vision-quality">
                    <span>${item.deviceId || '视觉样本'}</span>
                    <span class="quality-score ${qualityError ? 'error' : ''}">${qualityText}</span>
                </div>
                <div class="vision-time">${item.time}</div>
            </div>
        `;

        const img = div.querySelector('img');
        img.addEventListener('click', () => openVisionPreview(item.imgUrl, `${item.deviceId || ''} ${qualityText} · ${item.time}`));
        img.addEventListener('error', () => {
            div.querySelector('.vision-image-wrap').innerHTML = '<div class="empty-state">图片加载失败</div>';
        });
        container.appendChild(div);
    });

    requestAnimationFrame(() => {
        container.scrollLeft = container.scrollWidth;
    });
}

function openVisionPreview(src, caption) {
    const lightbox = document.getElementById('image-lightbox');
    const image = document.getElementById('lightbox-image');
    const text = document.getElementById('lightbox-caption');
    if (!lightbox || !image || !text) return;
    image.src = src;
    text.innerText = caption || '';
    lightbox.classList.add('open');
    lightbox.setAttribute('aria-hidden', 'false');
}

function closeVisionPreview() {
    const lightbox = document.getElementById('image-lightbox');
    if (!lightbox) return;
    lightbox.classList.remove('open');
    lightbox.setAttribute('aria-hidden', 'true');
}

window.closeVisionPreview = closeVisionPreview;

document.addEventListener('DOMContentLoaded', () => {
    initCharts();
    setupHorizontalScroll('vision-gallery-container');
    renderVisionGallery([]);
    window.addEventListener('resize', resizeCharts);
    document.addEventListener('keydown', (event) => {
        if (event.key === 'Escape') closeVisionPreview();
    });
    const lightbox = document.getElementById('image-lightbox');
    if (lightbox) {
        lightbox.addEventListener('click', (event) => {
            if (event.target === lightbox) closeVisionPreview();
        });
    }
});
