let currentFieldIndex = 0;
let fieldList = [];
let idleTimer = null;
let dataRefreshTimer = null; 
const IDLE_TIMEOUT = 60000;  
const REFRESH_RATE = 6000;   

const SCALE_NAMES = ["5分钟", "2小时", "1天", "1周"];
let scales = { sugar: 0, temp: 0, hum: 0, light: 0, spoil: 0 };

window.currentSelectedFruitId = null;
window.currentFieldId = null;

document.addEventListener('DOMContentLoaded', async () => {
    document.addEventListener('mousemove', resetIdleTimer);
    document.addEventListener('mousedown', resetIdleTimer);
    document.addEventListener('keypress', resetIdleTimer);

    await fetchFieldList();
    if (fieldList.length > 0) {
        loadFieldData(currentFieldIndex);
        startAutoSwitch();
        startDataRefresh(); 
    } else {
        document.getElementById('field-info-display').innerHTML = `<span style="color:red;">未查询到产区信息</span>`;
    }
});

window.forceRefresh = function() {
    console.log("手动强制刷新数据！");
    resetIdleTimer();
    loadFieldData(currentFieldIndex, true);
}

function changeScale(scaleKey, step) {
    resetIdleTimer();
    let current = scales[scaleKey];
    current += step;
    if (current < 0) current = 0;
    if (current > 3) current = 3;

    if (scales[scaleKey] !== current) {
        scales[scaleKey] = current;
        document.getElementById(`text-${scaleKey}`).innerText = `刻度: ${SCALE_NAMES[current]}`;
        window.reloadAllChartsData();
    }
}

function switchField(step) {
    if (fieldList.length === 0) return;
    resetIdleTimer();
    currentFieldIndex = (currentFieldIndex + step + fieldList.length) % fieldList.length;
    window.currentSelectedFruitId = null; 
    loadFieldData(currentFieldIndex);
}

function resetIdleTimer() { clearTimeout(idleTimer); startAutoSwitch(); }
function startAutoSwitch() { idleTimer = setTimeout(() => { switchField(1); }, IDLE_TIMEOUT); }

function startDataRefresh() {
    clearInterval(dataRefreshTimer);
    dataRefreshTimer = setInterval(() => {
        if (window.currentFieldId) { window.reloadAllChartsData(); }
    }, REFRESH_RATE);
}

async function fetchFieldList() {
    try {
        const res = await fetch('/api/admin/field/list');
        const json = await res.json();
        if (json.code === 200) fieldList = json.data.list.map(item => item.field_id);
    } catch (e) { console.error("获取产区列表失败", e); }
}

async function loadFieldData(index, isForce = false) {
    const fieldId = fieldList[index];
    window.currentFieldId = fieldId;
    document.getElementById('field-info-display').innerHTML = `当前查看：<span style="color:#f1c40f;">产区 ${fieldId}</span> (${index + 1}/${fieldList.length})`;

    try {
        const resFruit = await fetch(`/api/admin/fruit/list?field_id=${fieldId}`);
        const jsonFruit = await resFruit.json();
        
        if (jsonFruit.code === 200) {
            renderFruitGrid(jsonFruit.data);
            
            if (jsonFruit.data.length > 0) {
                if (!window.currentSelectedFruitId || !isForce) {
                    window.currentSelectedFruitId = jsonFruit.data[0].device_id;
                }
                
                setTimeout(() => { 
                    const cards = document.querySelectorAll('.fruit-item');
                    cards.forEach(card => {
                        const idEl = card.querySelector('.fruit-id');
                        if (idEl && idEl.innerText === window.currentSelectedFruitId) {
                            card.classList.add('active');
                        }
                    });
                }, 50);
            } else {
                window.currentSelectedFruitId = null; // 如果数据库没数据，置空
            }
        }
        window.reloadAllChartsData();
    } catch (e) { console.error("加载数据失败", e); }
}

function getTimeRange(scaleLevel) {
    let now = new Date().getTime(); 
    let interval = 0;
    
    if (scaleLevel === 0) interval = 5 * 60 * 1000;              
    else if (scaleLevel === 1) interval = 2 * 60 * 60 * 1000;    
    else if (scaleLevel === 2) interval = 24 * 60 * 60 * 1000;   
    else if (scaleLevel === 3) interval = 7 * 24 * 60 * 60 * 1000; 
    
    let past = now - (11 * interval); 
    return { min: past, max: now, interval: interval };
}

function updateLastRefreshTime() {
    let now = new Date();
    let h = String(now.getHours()).padStart(2, '0');
    let m = String(now.getMinutes()).padStart(2, '0');
    let s = String(now.getSeconds()).padStart(2, '0');
    
    // 增加安全判断：只有元素存在时才修改
    let timeEl = document.getElementById('last-update-text');
    if (timeEl) {
        timeEl.innerText = `数据同步于 ${h}:${m}:${s}`;
        timeEl.style.color = "#FDEEEF";
    }
}

window.reloadAllChartsData = async function() {
    if (!window.currentFieldId) return;

    try {
        const resEnv = await fetch(`/api/admin/field/environment?field_id=${window.currentFieldId}`);
        const jsonEnv = await resEnv.json();
        
        if (jsonEnv.code === 200) {
            const rawData = jsonEnv.data;
            const tempData = rawData.map(d => [d.timestamp * 1000, d.temperature]);
            const humData = rawData.map(d => [d.timestamp * 1000, d.humidity]);
            const lightData = rawData.map(d => [d.timestamp * 1000, d.light]);
            
            // 如果后端没有返回 spoilage 字段，给个 0.0 防止报错
            const spoilData = rawData.map(d => [d.timestamp * 1000, d.spoilage !== undefined ? d.spoilage : 0.0]);
            
            const tempRange = getTimeRange(scales.temp);
            const humRange = getTimeRange(scales.hum);
            const lightRange = getTimeRange(scales.light);
            const spoilRange = getTimeRange(scales.spoil);

            renderEnvHistoryCharts(tempData, humData, lightData, tempRange, humRange, lightRange);
            renderSpoilHistoryChart(spoilData, spoilRange);
        }

        if (window.currentSelectedFruitId) {
            const resFruit = await fetch(`/api/admin/fruit/history?device_id=${window.currentSelectedFruitId}`);
            const jsonFruit = await resFruit.json();
            
            if (jsonFruit.code === 200) {
                const sugarData = jsonFruit.data.map(d => [d.timestamp * 1000, d.sugar_brix]);
                const sugarRange = getTimeRange(scales.sugar);
                renderFruitHistoryChart(sugarData, window.currentSelectedFruitId, sugarRange, 15.0);
                
                // 从历史数据中提取视觉数据（quality_level 和 image_url）
                const visionData = jsonFruit.data
                    .filter(d => d.quality_level !== undefined && d.image_url)
                    .map(d => {
                        const date = new Date(d.timestamp * 1000);
                        const monthDay = `${(date.getMonth()+1).toString().padStart(2,'0')}.${date.getDate().toString().padStart(2,'0')}`;
                        const timeStr = `${date.getHours().toString().padStart(2,'0')}:${date.getMinutes().toString().padStart(2,'0')}`;
                        return {
                            imgUrl: `http://47.107.41.102:9000/${d.image_url}`,
                            quality: d.quality_level >= 0 ? d.quality_level : 'N/A',
                            time: `${monthDay}-${timeStr}`
                        };
                    });
                renderVisionGallery(visionData);
            }
        } else {
             renderFruitHistoryChart([], "无设备", getTimeRange(scales.sugar), 15.0);
             renderVisionGallery([]);
        }
        
        updateLastRefreshTime();

    } catch (e) {
        console.error("图表渲染失败", e);
        let timeEl = document.getElementById('last-update-text');
        if (timeEl) {
            timeEl.innerText = "数据同步失败(网络断开)";
            timeEl.style.color = "#E24B4A";
        }        
    }
}