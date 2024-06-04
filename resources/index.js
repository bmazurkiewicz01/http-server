document.addEventListener('DOMContentLoaded', () => {
    const btn = document.getElementById('add-h2-btn');
    btn.addEventListener('click', () => {
        const h2 = document.createElement('h2');
        h2.textContent = 'test H2 dolaczony przez javascript';
        document.body.appendChild(h2);
    });
});
