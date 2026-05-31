const user = checkAuth();
if (user) { initSidebar(); if (!hasPerm('message.manage')) { document.querySelector('.main-content').innerHTML = '<div class="card" style="text-align:center;padding:60px"><h2>权限不足</h2></div>'; } }

function roleLabel(r) { const m={admin:'管理员',root:'管理员',user:'普通用户'}; return m[r]||'用户'; }

let currentChatUserId = null;
let feedbackPollTimer = null;

async function loadConversations() {
    const res = await get('/api/message/conversations');
    const container = document.getElementById('conversation-list');
    if (!res || !res.ok) { container.innerHTML = '<div style="color:#999">加载失败</div>'; return; }
    const convos = res.data.conversations || [];
    if (convos.length === 0) { container.innerHTML = '<div style="color:#999">暂无用户消息</div>'; return; }
    container.innerHTML = convos.map(c => {
        const name = c.truename || c.username;
        const initial = name.charAt(0).toUpperCase();
        const msg = (c.last_message || '').substring(0, 30);
        const time = (c.last_time || '').split(' ')[1] || '';
        const unread = c.unread_count > 0 ? `<span class="badge badge-danger" style="font-size:10px;margin-left:4px">${c.unread_count}</span>` : '';
        const active = c.user_id === currentChatUserId ? 'background:#e6f7ff;' : '';
        return `<div onclick="selectConversation(${c.user_id})" style="padding:10px;border-bottom:1px solid #f0f0f0;cursor:pointer;${active}">
            <div style="display:flex;align-items:center;gap:8px;">
                <div style="width:32px;height:32px;border-radius:50%;background:#00aa7f;color:#fff;display:flex;align-items:center;justify-content:center;font-size:13px;flex-shrink:0">${initial}</div>
                <div style="flex:1;overflow:hidden;">
                    <div style="font-weight:600;font-size:13px;">${escapeHtml(name)}${unread}</div>
                    <div style="font-size:11px;color:#999;overflow:hidden;text-overflow:ellipsis;white-space:nowrap;">${escapeHtml(msg)}</div>
                </div>
                <div style="font-size:10px;color:#bbb;flex-shrink:0">${time.substring(0,5)}</div>
            </div>
        </div>`;
    }).join('');
    if (feedbackPollTimer) clearInterval(feedbackPollTimer);
    feedbackPollTimer = setInterval(() => {
        loadConversations();
        if (currentChatUserId) loadFeedbackMessages(currentChatUserId);
    }, 3000);
}

async function selectConversation(userId) {
    currentChatUserId = userId;
    const res = await get('/api/message/conversations');
    if (res && res.ok) {
        const convos = res.data.conversations || [];
        const c = convos.find(x => x.user_id === userId);
        if (c) {
            const name = c.truename || c.username;
            const initial = name.charAt(0).toUpperCase();
            document.getElementById('feedback-avatar').textContent = initial;
            document.getElementById('feedback-username').textContent = name;
            document.getElementById('feedback-userdetail').textContent = `ID: ${c.user_id}`;
            document.getElementById('feedback-truename').textContent = c.truename || '-';
            document.getElementById('feedback-telephone').textContent = c.telephone || '-';
            document.getElementById('feedback-role').textContent = roleLabel(c.role);
            document.getElementById('feedback-balance').textContent = '¥' + parseFloat(c.balance||0).toFixed(2);
            document.getElementById('feedback-raw-username').textContent = c.username || '-';
            document.getElementById('feedback-created').textContent = formatDateTime(c.created_at).substring(0, 10);
            document.getElementById('feedback-user-info').style.display = '';
        }
    }
    loadFeedbackMessages(userId);
    loadConversations();
}

async function loadFeedbackMessages(userId) {
    const res = await get('/api/message/history?user_id=' + userId);
    const container = document.getElementById('feedback-messages');
    if (!res || !res.ok) { container.innerHTML = '<div style="text-align:center;color:#999">加载失败</div>'; return; }
    const messages = res.data.messages || [];
    if (messages.length === 0) { container.innerHTML = '<div style="text-align:center;color:#999">暂无消息</div>'; return; }
    let html = '', lastDate = '';
    messages.forEach(m => {
        const date = (m.created_at || '').split(' ')[0];
        const time = (m.created_at || '').split(' ')[1] || '';
        if (date && date !== lastDate) { html += `<div style="text-align:center;font-size:11px;color:#999;padding:4px 0">${date}</div>`; lastDate = date; }
        const isSent = m.sender_id === user.id;
        html += `<div style="display:flex;${isSent ? 'justify-content:flex-end' : 'justify-content:flex-start'};margin-bottom:6px;">
            <div style="max-width:70%;padding:8px 12px;border-radius:10px;font-size:13px;${isSent ? 'background:#00aa7f;color:#fff;border-bottom-right-radius:2px' : 'background:#fff;color:#333;border-bottom-left-radius:2px;box-shadow:0 1px 2px rgba(0,0,0,0.05)'}">
                ${escapeHtml(m.content)}
                <div style="font-size:10px;margin-top:2px;opacity:0.7">${time.substring(0,5)}</div>
            </div>
        </div>`;
    });
    container.innerHTML = html;
    container.scrollTop = container.scrollHeight;
}

async function sendAdminReply() {
    const input = document.getElementById('feedback-reply-input');
    const content = input.value.trim();
    if (!content || !currentChatUserId) return;
    input.value = '';
    const res = await post('/api/message/send', { receiver_id: currentChatUserId, content });
    if (res && res.ok) { loadFeedbackMessages(currentChatUserId); loadConversations(); }
}

document.getElementById('feedback-reply-input')?.addEventListener('keydown', e => {
    if (e.key === 'Enter') sendAdminReply();
});

loadConversations();
