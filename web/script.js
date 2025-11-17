document.getElementById('evaluacionForm').addEventListener('submit', function(event) {
    event.preventDefault(); // Detiene el envío estándar del formulario

    // 1. Recolectar datos del formulario
    const signosVitales = {
        fc_lpm: parseFloat(document.getElementById('fc_lpm').value),
        fr_rpm: parseFloat(document.getElementById('fr_rpm').value),
        temp_c: parseFloat(document.getElementById('temp_c').value),
        spo2_pct: parseFloat(document.getElementById('spo2_pct').value),
        pas_mmhg: parseFloat(document.getElementById('pas_mmhg').value),
        peso_kg: parseFloat(document.getElementById('peso_kg').value),
        talla_m: parseFloat(document.getElementById('talla_m').value)
    };

    const sintomas = {
        fiebre_escalofrios: document.getElementById('fiebre_escalofrios').checked,
        dolor_toracico: document.getElementById('dolor_toracico').checked,
        disnea: parseInt(document.getElementById('disnea').value),
        dolor_abdominal: parseInt(document.getElementById('dolor_abdominal').value),
        cefalea_subita: document.getElementById('cefalea_subita').checked,
        palidez_sudoracion: document.getElementById('palidez_sudoracion').checked
    };

    const datosParaEnviar = {
        signos: signosVitales,
        sintomas: sintomas
    };

    // 2. Enviar datos al servidor C++ (API)
    fetch('/api/evaluar', {
        method: 'POST',
        headers: {
            'Content-Type': 'application/json'
        },
        body: JSON.stringify(datosParaEnviar)
    })
    .then(response => {
        if (!response.ok) {
            throw new Error('Error en el servidor (' + response.status + ')');
        }
        return response.json();
    })
    .then(data => {
        // 3. Mostrar resultados
        mostrarResultados(data);
        cargarHistorial();
    })
    .catch(error => {
        console.error('Error al evaluar:', error);
        alert('Ocurrió un error al contactar al servidor de evaluación.');
    });
});

function mostrarResultados(data) {
    const resultadoDiv = document.getElementById('resultado');
    const nivelGlobalH3 = document.getElementById('nivel-global');
    const listaUl = document.getElementById('lista-resultados');

    // Limpiar resultados anteriores
    listaUl.innerHTML = '';

    // Asignar clase de color al nivel global
    const nivelClase = 'nivel-' + data.nivel_global.toLowerCase();
    nivelGlobalH3.className = nivelClase; // Aplica el estilo de color
    nivelGlobalH3.textContent = `Nivel Global: ${data.nivel_global}`;

    // Listar los resultados detallados
    data.items.forEach(item => {
        const li = document.createElement('li');
        const itemNivelClase = 'nivel-' + item.nivel.toLowerCase();
        
        li.innerHTML = `<span class="${itemNivelClase}">${item.nombre} (${item.nivel}):</span> ${item.explicacion}`;
        listaUl.appendChild(li);
    });

    resultadoDiv.style.display = 'block'; // Mostrar la sección de resultados
}

// --- Historial ---
function cargarHistorial() {
    const tbody = document.getElementById('tabla-historial');
    tbody.innerHTML = '<tr><td colspan="3">Cargando...</td></tr>';
    fetch('/api/evaluaciones?limit=20')
        .then(r => {
            if (!r.ok) throw new Error('HTTP ' + r.status);
            return r.json();
        })
        .then(items => {
            if (!Array.isArray(items)) throw new Error('Respuesta inesperada');
            if (items.length === 0) {
                tbody.innerHTML = '<tr><td colspan="3">Sin registros aún</td></tr>';
                return;
            }
            tbody.innerHTML = '';
            items.forEach(row => {
                const tr = document.createElement('tr');
                const nivelClase = 'nivel-' + (row.nivel_global || 'normal').toLowerCase();
                const detalle = row.reporte && row.reporte.items
                    ? row.reporte.items.map(it => `${it.nombre}: ${it.nivel}`).join(' | ')
                    : '—';
                tr.innerHTML = `
                    <td>${row.fecha || ''}</td>
                    <td class="nivel ${nivelClase}">${row.nivel_global || ''}</td>
                    <td>${detalle}</td>
                `;
                tbody.appendChild(tr);
            });
        })
        .catch(err => {
            console.error('Error al cargar historial', err);
            tbody.innerHTML = '<tr><td colspan="3">Error al cargar historial</td></tr>';
        });
}

// Cargar historial al inicio y tras cada nueva evaluación
document.addEventListener('DOMContentLoaded', cargarHistorial);
