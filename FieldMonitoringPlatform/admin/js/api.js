// admin/js/api.js
const BASE_URL = "http://47.107.41.102:9000/api"; 

// 登录接口调用
async function login(username, password) {
    try {
        const response = await fetch(`${BASE_URL}/admin/login`, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ username, password })
        });
        
        const resData = await response.json();
        
        if (resData.code === 200) {
            // 登录成功，将返回的 cookie(session_id) 的控制权交给浏览器
            // 跳转到 dashboard
            window.location.href = "dashboard.html";
        } else {
            // 显示后端返回的错误信息
            showError(resData.msg);
        }
    } catch (error) {
        showError("网络连接失败，请检查服务器是否启动");
        console.error("Login Error:", error);
    }
}

function showError(msg) {
    const errorEl = document.getElementById('error-msg');
    if (errorEl) {
        errorEl.innerText = msg;
        errorEl.style.display = 'block';
    }
}