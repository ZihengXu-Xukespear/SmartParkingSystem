// 全局：拦截已注销账号，注销后跳转到 index.html
function checkBannedUser(username) {
    let banned = JSON.parse(localStorage.getItem("banned_users") || "[]");
    if (banned.includes(username)) {
        alert("账号已注销，无法使用系统！");
        localStorage.clear();
        location.href = "/index.html"; // 跳转到真正的登录页
        return true;
    }
    return false;
}

const user = checkAuth();
//  在这里调用拦截！获取用户后立刻检查是否注销
if (user) {
    checkBannedUser(user.username);
    initSidebar();
}

function loadProfile() {
    document.getElementById('prof-username').value = user.username || '';
    document.getElementById('prof-phone').value = user.telephone || '';
    document.getElementById('prof-truename').value = user.truename || '';
    document.getElementById('prof-role').value = (user.role === 'admin' || user.role === 'root' ? '管理员' : '普通用户');
}

async function updateProfile() {
    const phone = document.getElementById('prof-phone').value.trim();
    const truename = document.getElementById('prof-truename').value.trim();
    const password = document.getElementById('prof-new-pass').value;
    const body = { id: user.id, username: user.username, telephone: phone, truename, role: user.role };
    if (password) body.password = password;

    const res = await put('/api/user/update', body);
    if (res && res.ok) {
        user.telephone = phone;
        user.truename = truename;
        sessionStorage.setItem('user', JSON.stringify(user));
        initSidebar();
        showSuccess('alert-box', '个人信息已更新');
    } else {
        showError('alert-box', res?.data?.error || '更新失败');
    }
}

async function loadBalance() {
    if (!hasPerm('balance.view')) return;
    const res = await get('/api/balance');
    const balEl = document.getElementById('prof-balance');
    const txEl = document.getElementById('prof-transactions');
    if (res && res.ok) {
        if (balEl) balEl.textContent = '¥' + parseFloat(res.data.balance).toFixed(2);
        if (txEl && res.data.transactions) {
            txEl.innerHTML = res.data.transactions.slice(0, 20).map(t =>
                `<div style="padding:4px 0;border-bottom:1px solid #f0f0f0;font-size:12px">
                    <span style="color:${t.amount > 0 ? '#52c41a' : '#ff4d4f'}">${t.amount > 0 ? '+' : ''}${parseFloat(t.amount).toFixed(2)}</span>
                    <span style="color:#999;margin-left:8px">${escapeHtml(t.description)}</span>
                    <span style="float:right;color:#bbb;font-size:11px">${formatDateTime(t.created_at)}</span>
                </div>`
            ).join('') || '<p style="color:#999;font-size:12px">暂无交易记录</p>';
        }
    }
}

async function loadPasses() {
    const container = document.getElementById('prof-passes');
    if (!container) return;

    try {
        const res = await get(`/api/parking/monthly-passes?user_id=${user.id}`);
        if (!res || !res.ok || !res.data.passes) {
            container.innerHTML = '<div class="package-empty">暂无套餐</div>';
            return;
        }

        let allPasses = res.data.passes.filter(p => p.user_id === user.id);
        let mergedMap = {};
        allPasses.forEach(p => {
            let key = p.license_plate;
            if (!mergedMap[key]) {
                mergedMap[key] = {
                    license_plate: p.license_plate,
                    pass_type: p.pass_type,
                    start_date: p.start_date,
                    end_date: p.end_date,
                    fee: parseFloat(p.fee),
                    is_active: p.is_active
                };
            } else {
                if (!mergedMap[key].pass_type.includes(p.pass_type)) {
                    mergedMap[key].pass_type += " + " + p.pass_type;
                }
                if (p.end_date > mergedMap[key].end_date) {
                    mergedMap[key].end_date = p.end_date;
                }
                mergedMap[key].fee += parseFloat(p.fee);
            }
        });

        let finalList = Object.values(mergedMap);
        container.innerHTML = finalList.map(item => `
            <div class="package-item">
                <div class="package-type">
                    ${item.license_plate} • ${escapeHtml(item.pass_type)}
                </div>
                <div class="package-time">
                    生效时间：${formatDate(item.start_date)}<br>
                    到期时间：${formatDate(item.end_date)}
                </div>
                <div style="margin-top:6px;display:flex;justify-content:space-between;align-items:center">
                    <span style="font-size:12px;color:#666">¥${item.fee.toFixed(2)}</span>
                    <span class="badge ${item.is_active ? 'badge-success' : 'badge-danger'}" style="font-size:10px">
                        ${item.is_active ? '有效' : '已过期'}
                    </span>
                </div>
            </div>
        `).join('');

    } catch (err) {
        console.error('加载套餐失败：', err);
        container.innerHTML = '<div class="package-empty">加载失败</div>';
    }
}

loadProfile();
loadBalance();
loadPasses();