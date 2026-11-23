// script.js
document.addEventListener('DOMContentLoaded', init);

async function init() {
    await loadDatasets();
    document.getElementById('new-dataset-btn').addEventListener('click', createCustomDataset);
}

async function loadDatasets() {
    const res = await fetch('/api/datasets');
    const data = await res.json();
    const list = document.getElementById('dataset-list');
    list.innerHTML = '';
    data.datasets.forEach(ds => {
        const li = document.createElement('li');
        li.textContent = ds;
        // add delete icon
        const del = document.createElement('span');
        del.innerHTML = '&#128465;'; // trash can unicode
        del.className = 'delete-icon';
        del.title = 'Delete dataset';
        // Delete icon click handler with robust error handling
        del.addEventListener('click', async (e) => {
            e.stopPropagation(); // prevent li click triggering selection
            // Prevent multiple simultaneous deletions
            if (del.dataset.deleting === 'true') return;
            del.dataset.deleting = 'true';
            try {
                const confirmed = confirm(`Delete dataset "${ds}"? This will permanently remove its CSV file.`);
                if (!confirmed) return;
                const resp = await fetch('/api/dataset/delete', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ name: ds })
                });
                if (!resp.ok) {
                    const text = await resp.text();
                    alert(`Failed to delete dataset. HTTP ${resp.status}: ${text}`);
                    return;
                }
                const result = await resp.json();
                if (result.success) {
                    // If the deleted dataset is currently open, clear the UI
                    if (document.getElementById('title').textContent.startsWith(ds + ' Dataset')) {
                        document.getElementById('title').textContent = 'Dashboard';
                        document.getElementById('form-container').classList.add('hidden');
                        document.getElementById('records-container').classList.add('hidden');
                        document.getElementById('total-records').textContent = '0';
                    }
                    await loadDatasets();
                } else {
                    alert('Error deleting dataset: ' + result.message);
                }
            } catch (err) {
                console.error('Delete dataset exception:', err);
                alert('Unexpected error deleting dataset: ' + err.message);
            } finally {
                del.dataset.deleting = 'false';
            }
        });
        li.appendChild(del);
        li.addEventListener('click', () => selectDataset(ds));
        list.appendChild(li);
    });
    document.getElementById('total-datasets').textContent = data.datasets.length;
}

async function selectDataset(name) {
    document.getElementById('title').textContent = name + ' Dataset';
    const metaRes = await fetch(`/api/dataset/meta?name=${encodeURIComponent(name)}`);
    const meta = await metaRes.json();
    // Build dynamic form
    const formContainer = document.getElementById('form-container');
    formContainer.innerHTML = '';
    const form = document.createElement('form');
    form.id = 'record-form';
    meta.columns.forEach(col => {
        const div = document.createElement('div');
        const label = document.createElement('label');
        label.textContent = col.name + ':';
        const input = document.createElement('input');
        input.name = col.name;
        input.required = true;
        switch (col.type) {
            case 'Number': input.type = 'number'; break;
            case 'Email': input.type = 'email'; break;
            case 'Date': input.type = 'date'; break;
            default: input.type = 'text';
        }
        div.appendChild(label);
        div.appendChild(input);
        form.appendChild(div);
    });
    const submit = document.createElement('button');
    submit.type = 'submit';
    submit.textContent = 'Add Record';
    form.appendChild(submit);
    form.addEventListener('submit', e => { e.preventDefault(); addRecord(name, meta.columns); });
    formContainer.appendChild(form);
    formContainer.classList.remove('hidden');
    // Load records table
    await loadRecords(name);
}

async function addRecord(dataset, columns) {
    const form = document.getElementById('record-form');
    const formData = new FormData(form);
    const record = {};
    columns.forEach(col => {
        record[col.name] = formData.get(col.name);
    });
    const res = await fetch('/api/record/add', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ dataset, record })
    });
    const result = await res.json();
    if (result.success) {
        alert('Record added');
        form.reset();
        await loadRecords(dataset);
    } else {
        alert('Error: ' + result.message);
    }
}

async function loadRecords(dataset) {
    const res = await fetch(`/api/records?dataset=${encodeURIComponent(dataset)}`);
    const data = await res.json();
    const container = document.getElementById('records-container');
    const head = document.getElementById('table-head');
    const body = document.getElementById('table-body');
    head.innerHTML = '';
    body.innerHTML = '';
    if (data.columns && data.records) {
        const tr = document.createElement('tr');
        data.columns.forEach(col => {
            const th = document.createElement('th');
            th.textContent = col;
            tr.appendChild(th);
        });
        const thDel = document.createElement('th');
        thDel.textContent = 'Actions';
        tr.appendChild(thDel);
        head.appendChild(tr);
        data.records.forEach((row, idx) => {
            const tr = document.createElement('tr');
            row.forEach(cell => {
                const td = document.createElement('td');
                td.textContent = cell;
                tr.appendChild(td);
            });
            const td = document.createElement('td');
            const delBtn = document.createElement('button');
            delBtn.textContent = 'Delete';
            delBtn.addEventListener('click', () => deleteRecord(dataset, idx));
            td.appendChild(delBtn);
            tr.appendChild(td);
            body.appendChild(tr);
        });
        document.getElementById('total-records').textContent = data.records.length;
    }
    container.classList.remove('hidden');
    document.getElementById('export-csv').onclick = () => { window.location = `/api/records/export?dataset=${encodeURIComponent(dataset)}`; };
}

async function deleteRecord(dataset, index) {
    if (!confirm('Delete this record?')) return;
    const res = await fetch('/api/record/delete', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ dataset, index })
    });
    const result = await res.json();
    if (result.success) {
        await loadRecords(dataset);
    } else {
        alert('Error: ' + result.message);
    }
}

async function createCustomDataset() {
    const name = prompt('Enter dataset name (alphanumeric & underscores only)');
    if (!name) return;
    const columns = [];
    while (true) {
        const colName = prompt('Column name (Leave empty to finish)');
        if (!colName) break;
        const type = prompt('Column type (Text, Number, Email, Date)');
        columns.push({ name: colName, type: type || 'Text' });
    }
    if (columns.length === 0) { alert('At least one column required'); return; }
    const res = await fetch('/api/dataset/create', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ name, columns })
    });
    const result = await res.json();
    if (result.success) {
        await loadDatasets();
    } else {
        alert('Error: ' + result.message);
    }
}
