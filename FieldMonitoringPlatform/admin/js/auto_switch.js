let currentFieldIndex = 0;
let fieldList = [];
let idleTimer = null;
let dataRefreshTimer = null;
let isReloading = false;
let currentFieldFruitCache = [];

const IDLE_TIMEOUT = 60000;
const REFRESH_RATE = 6000;
const SCALE_NAMES = ['5 分钟', '2 小时', '1 天', '1 周'];
const scales = { sugar: 0, temp: 0, hum: 0, light: 0, spoil: 0 };

window.currentSelectedFruitId = null;
window.currentFieldId = null;

document.addEventListener('DOMContentLoaded', async () => {
    ['mousemove', 'mousedown', 'keypress', 'touchstart'].forEach(eventName => {
        document.addEventListener(eventName, resetIdleTimer, { passive: true });
    });

    updateScaleLabels();
    await fetchFieldList();

    if (fieldList.length > 0) {
        await loadFieldData(currentFieldIndex);
        startAutoSwitch();
        startDataRefresh();
    } else {
        setFieldDisplay('暂无产区', '请检查登录状态或数据库配置');
        setLastUpdate('未获取到产区', true);
    }
});

window.forceRefresh = function() {
    resetIdleTimer();
    loadFieldData(currentFieldIndex, true);
};

window.changeScale = function(scaleKey, step) {
    resetIdleTimer();
    if (!Object.prototype.hasOwnProperty.call(scales, scaleKey)) return;
    const next = Math.max(0, Math.min(3, scales[scaleKey] + step));
    if (next === scales[scaleKey]) return;
    scales[scaleKey] = next;
    updateScaleLabels();
    window.reloadAllChartsData();
};

window.switchField = function(step) {
    if (fieldList.length === 0) return;
    resetIdleTimer();
    currentFieldIndex = (currentFieldIndex + step + fieldList.length) % fieldList.length;
    window.currentSelectedFruitId = null;
    loadFieldData(currentFieldIndex);
};

function resetIdleTimer() {
    clearTimeout(idleTimer);
    startAutoSwitch();
}

window.resetIdleTimer = resetIdleTimer;

function startAutoSwitch() {
    clearTimeout(idleTimer);
    idleTimer = setTimeout(() => {
        if (fieldList.length > 1) window.switchField(1);
    }, IDLE_TIMEOUT);
}

function startDataRefresh() {
    clearInterval(dataRefreshTimer);
    dataRefreshTimer = setInterval(() => {
        if (window.currentFieldId) window.reloadAllChartsData();
    }, REFRESH_RATE);
}

async function fetchFieldList() {
    try {
        const response = await fetch('/api/admin/field/list', { credentials: 'same-origin' });
        const result = await response.json();
        if (result.code === 200 && result.data && Array.isArray(result.data.list)) {
            fieldList = result.data.list.map(item => item.field_id);
        } else {
            fieldList = [];
        }
    } catch (error) {
        console.error('获取产区列表失败', error);
        fieldList = [];
    }
}

async function loadFieldData(index, isForce = false) {
    if (!fieldList[index]) return;
    const fieldId = fieldList[index];
    window.currentFieldId = fieldId;
    setFieldDisplay(fieldId, `${index + 1}/${fieldList.length}`);
    setLastUpdate(isForce ? '正在强制刷新...' : '正在加载...');

    try {
        const response = await fetch(`/api/admin/fruit/list?field_id=${encodeURIComponent(fieldId)}`, { credentials: 'same-origin' });
        const result = await response.json();

        if (result.code === 200 && Array.isArray(result.data)) {
            currentFieldFruitCache = result.data;
            if (result.data.length > 0) {
                const selectedStillExists = result.data.some(item => item.device_id === window.currentSelectedFruitId);
                if (!window.currentSelectedFruitId || !selectedStillExists || !isForce) {
                    window.currentSelectedFruitId = result.data[0].device_id;
                }
            } else {
                window.currentSelectedFruitId = null;
            }
            renderFruitGrid(result.data);
            await window.reloadAllChartsData();
        } else {
            currentFieldFruitCache = [];
            renderFruitGrid([]);
            setLastUpdate('设备数据异常', true);
        }
    } catch (error) {
        console.error('加载产区设备失败', error);
        setLastUpdate('加载失败', true);
    }
}

async function refreshFruitListSilently() {
    if (!window.currentFieldId) return;
    try {
        const response = await fetch(`/api/admin/fruit/list?field_id=${encodeURIComponent(window.currentFieldId)}`, { credentials: 'same-origin' });
        const result = await response.json();
        if (result.code === 200 && Array.isArray(result.data)) {
            currentFieldFruitCache = result.data;
            if (result.data.length && !result.data.some(item => item.device_id === window.currentSelectedFruitId)) {
                window.currentSelectedFruitId = result.data[0].device_id;
            }
            if (!result.data.length) window.currentSelectedFruitId = null;
            renderFruitGrid(result.data);
        }
    } catch (error) {
        console.error('静默刷新设备列表失败', error);
    }
}

function getTimeRange(scaleLevel) {
    const now = Date.now();
    const intervals = [
        5 * 60 * 1000,
        2 * 60 * 60 * 1000,
        24 * 60 * 60 * 1000,
        7 * 24 * 60 * 60 * 1000
    ];
    const interval = intervals[scaleLevel] || intervals[0];
    return { min: now - (11 * interval), max: now, interval };
}

window.reloadAllChartsData = async function() {
    if (!window.currentFieldId || isReloading) return;
    isReloading = true;

    try {
        await refreshFruitListSilently();

        const envPromise = fetch(`/api/admin/field/environment?field_id=${encodeURIComponent(window.currentFieldId)}`, { credentials: 'same-origin' })
            .then(response => response.json());

        const fruitPromise = window.currentSelectedFruitId
            ? fetch(`/api/admin/fruit/history?device_id=${encodeURIComponent(window.currentSelectedFruitId)}`, { credentials: 'same-origin' }).then(response => response.json())
            : Promise.resolve({ code: 200, data: [] });

        const [envResult, fruitResult] = await Promise.all([envPromise, fruitPromise]);

        if (envResult.code === 200 && Array.isArray(envResult.data)) {
            const envData = envResult.data;
            const tempData = envData.map(item => [Number(item.timestamp) * 1000, Number(item.temperature)]);
            const humData = envData.map(item => [Number(item.timestamp) * 1000, Number(item.humidity)]);
            const lightData = envData.map(item => [Number(item.timestamp) * 1000, Number(item.light)]);
            const spoilData = envData.map(item => [Number(item.timestamp) * 1000, Number(item.spoilage || 0)]);

            renderEnvHistoryCharts(
                tempData,
                humData,
                lightData,
                getTimeRange(scales.temp),
                getTimeRange(scales.hum),
                getTimeRange(scales.light)
            );
            renderSpoilHistoryChart(spoilData, getTimeRange(scales.spoil));

            const alerts = envData.filter(item => Number(item.spoilage || 0) > 60).length;
            const statOnline = document.getElementById('stat-online');
            const statOffline = document.getElementById('stat-offline');
            const statRipe = document.getElementById('stat-ripe');
            updateDashboardStats({
                total: currentFieldFruitCache.length,
                online: statOnline ? Number(statOnline.innerText || 0) : 0,
                offline: statOffline ? Number(statOffline.innerText || 0) : 0,
                ripe: statRipe ? Number(statRipe.innerText || 0) : 0,
                alerts
            });
        }

        if (fruitResult.code === 200 && Array.isArray(fruitResult.data)) {
            const fruitData = fruitResult.data;
            const sugarData = fruitData.map(item => [Number(item.timestamp) * 1000, Number(item.sugar_brix)]);
            renderFruitHistoryChart(sugarData, window.currentSelectedFruitId, getTimeRange(scales.sugar), 15.0);

            const visionData = fruitData
                .filter(item => item.quality_level !== undefined && item.image_url)
                .map(item => {
                    const timestamp = Number(item.timestamp) * 1000;
                    return {
                        imgUrl: `/${String(item.image_url).replace(/^\/+/, '')}`,
                        quality: Number(item.quality_level) >= 0 ? Number(item.quality_level) : 'N/A',
                        time: formatDateTime(timestamp, 'short'),
                        deviceId: window.currentSelectedFruitId
                    };
                });
            renderVisionGallery(visionData);
        } else if (!window.currentSelectedFruitId) {
            renderFruitHistoryChart([], '', getTimeRange(scales.sugar), 15.0);
            renderVisionGallery([]);
        }

        setLastUpdate(`已更新 ${new Date().toLocaleTimeString('zh-CN', { hour12: false })}`);
    } catch (error) {
        console.error('刷新图表数据失败', error);
        setLastUpdate('刷新失败', true);
    } finally {
        isReloading = false;
    }
};

function updateScaleLabels() {
    Object.entries(scales).forEach(([key, value]) => {
        const el = document.getElementById(`text-${key}`);
        if (el) el.innerText = SCALE_NAMES[value];
    });
}

function setFieldDisplay(fieldId, meta) {
    const el = document.getElementById('field-info-display');
    if (!el) return;
    el.innerHTML = `${fieldId}<small style="display:block;color:#cbd5e1;font-size:11px;font-weight:700;">${meta || ''}</small>`;
}

function setLastUpdate(text, isError = false) {
    const el = document.getElementById('last-update-text');
    if (!el) return;
    el.innerText = text;
    el.style.color = isError ? '#d92d20' : '#667085';
    el.style.background = isError ? '#fff0ee' : '#f8fafc';
}
