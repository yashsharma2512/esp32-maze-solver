#pragma once

constexpr char WIFI_CONSOLE_PAGE[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MazeSolver Console</title>
<style>
  :root { color-scheme: dark; font: 16px system-ui, sans-serif; background: #12171d; color: #e9eff5; }
  body { max-width: 58rem; margin: 0 auto; padding: 1rem; }
  h1 { font-size: 1.35rem; margin: .25rem 0 .75rem; }
  pre { height: 56vh; overflow: auto; white-space: pre-wrap; overflow-wrap: anywhere; margin: 0 0 1rem;
        padding: .75rem; border: 1px solid #384553; border-radius: .4rem; background: #0b1016;
        font: .875rem/1.5 "IBM Plex Mono", Consolas, monospace; }
  form { display: flex; gap: .5rem; }
  input, button { font: inherit; padding: .55rem .7rem; border-radius: .3rem; border: 1px solid #556579; }
  input { flex: 1; min-width: 0; background: #191f27; }
  button { background: #9bd5f1; color: #10202a; cursor: pointer; }
  #status { min-height: 1.5em; margin: .5rem 0; color: #bccbdb; }
</style>
<h1>MazeSolver Console</h1>
<pre id="log" aria-label="Robot serial output" tabindex="0"></pre>
<form id="console">
  <input id="command" name="command" aria-label="Serial command" placeholder="camera, start, c, read, exit"
         maxlength="39" autocomplete="off" autocapitalize="off" spellcheck="false" required>
  <button type="submit">Send</button>
</form>
<p id="status" role="status"></p>
<script>
  const log = document.querySelector('#log');
  const input = document.querySelector('#command');
  const statusText = document.querySelector('#status');
  let cursor = 0;
  let disconnected = false;

  async function readLog() {
    try {
      const response = await fetch('/logs?from=' + cursor, {cache: 'no-store'});
      if (!response.ok) throw new Error();
      const text = await response.text();
      cursor = Number(response.headers.get('X-Log-Next'));
      const atBottom = log.scrollHeight - log.scrollTop - log.clientHeight < 24;
      if (text) log.textContent = (log.textContent + text).slice(-16384);
      if (atBottom) log.scrollTop = log.scrollHeight;
      if (disconnected) statusText.textContent = 'Connected.';
      disconnected = false;
    } catch {
      disconnected = true;
      statusText.textContent = 'Console disconnected. Retrying.';
    }
    setTimeout(readLog, 250);
  }

  document.querySelector('#console').addEventListener('submit', async (event) => {
    event.preventDefault();
    const command = input.value.trim();
    if (!command) return;
    try {
      const response = await fetch('/command', {
        method: 'POST',
        headers: {'X-Maze-Console': '1'},
        body: new URLSearchParams({command})
      });
      statusText.textContent = await response.text();
      if (response.ok && input.value.trim() === command) input.value = '';
    } catch {
      statusText.textContent = 'Command could not be sent.';
    }
  });
  readLog();
</script>
</html>
)HTML";
