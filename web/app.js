function copyText(button, text) {
  var original = button.innerHTML;
  var done = function() {
    button.innerHTML = '<i class="ti ti-check"></i>Copied';
    setTimeout(function() { button.innerHTML = original; }, 1500);
  };

  if (navigator.clipboard && navigator.clipboard.writeText) {
    navigator.clipboard.writeText(text).then(done);
    return;
  }

  var textarea = document.createElement('textarea');
  textarea.value = text;
  textarea.setAttribute('readonly', '');
  textarea.style.position = 'fixed';
  textarea.style.left = '-9999px';
  document.body.appendChild(textarea);
  textarea.select();
  document.execCommand('copy');
  document.body.removeChild(textarea);
  done();
}

document.querySelectorAll('[data-copy]').forEach(function(button) {
  button.addEventListener('click', function() {
    copyText(button, button.getAttribute('data-copy'));
  });
});
