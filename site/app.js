const tabs = Array.from(document.querySelectorAll('[data-platform]'));
function selectPlatform(tab, focus = false) {
  for (const item of tabs) {
    const selected = item === tab;
    item.setAttribute('aria-selected', String(selected));
    item.tabIndex = selected ? 0 : -1;
    document.getElementById(item.getAttribute('aria-controls')).hidden = !selected;
  }
  if (focus) tab.focus();
}
for (const tab of tabs) {
  tab.addEventListener('click', () => selectPlatform(tab));
  tab.addEventListener('keydown', event => {
    if (!['ArrowLeft', 'ArrowRight', 'Home', 'End'].includes(event.key)) return;
    event.preventDefault();
    const index = event.key === 'Home' ? 0 : event.key === 'End' ? tabs.length - 1
      : (tabs.indexOf(tab) + (event.key === 'ArrowRight' ? 1 : -1) + tabs.length) % tabs.length;
    selectPlatform(tabs[index], true);
  });
}
document.getElementById('copy-command').addEventListener('click', async () => {
  const current = tabs.find(tab => tab.getAttribute('aria-selected') === 'true');
  const command = document.getElementById(`${current.dataset.platform}-command`);
  const status = document.getElementById('copy-status');
  try {
    await navigator.clipboard.writeText(command.textContent);
    status.textContent = '命令已复制';
    document.getElementById('copy-command').textContent = '已复制 ✓';
    setTimeout(() => { document.getElementById('copy-command').textContent = '复制命令'; }, 2000);
  } catch {
    const range = document.createRange();
    range.selectNodeContents(command);
    const selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
    status.textContent = '命令已选中，请手动复制';
    document.getElementById('copy-command').textContent = '请手动复制';
  }
});
fetch('./logs/daheng-2026-10-04.log')
  .then(response => { if (!response.ok) throw new Error('Log unavailable'); return response.text(); })
  .then(text => { document.getElementById('log-content').textContent = text; })
  .catch(() => { document.getElementById('log-content').textContent = '暂时无法读取日志，请使用下方下载链接或前往 GitHub 查看原始文件。'; });
