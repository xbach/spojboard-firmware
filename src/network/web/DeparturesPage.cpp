#include "DeparturesPage.h"
#include "WebTemplates.h"
#include "WebUtils.h"

String buildDeparturesPage(const Departure* deps, int count)
{
    String html = FPSTR(HTML_HEADER);

    // Header with back link
    html += "<div class='header'><div class='header-top'>";
    html += "<div class='header-title'><h1>SpojBoard</h1>";
    html += "<div class='header-subtitle'>Cached Departures</div></div>";
    html += "<div class='action-bar'>";
    html += "<a href='/' class='action-btn' title='Back to Dashboard'>&#8592;</a>";
    html += "</div>";
    html += "</div></div>";

    html += "<div class='content'>";

    // Info banner
    html += "<div class='banner banner-info' style='margin:0 0 20px;'>";
    html += "<span class='status-dot'></span>";
    html += "<div id='dep-count'>Showing <b>" + String(count) + "</b> cached departure";
    if (count != 1)
        html += "s";
    html += " (max " + String(MAX_DEPARTURES) + ")</div>";
    html += "</div>";

    // Refresh button
    html += "<div style='margin-bottom:16px;'>";
    html += "<button type='button' class='secondary' id='refreshBtn' onclick='refreshDepartures()'>Refresh from Memory</button>";
    html += "</div>";

    // Table container (replaced by AJAX)
    html += "<div id='dep-table'>";

    if (count == 0)
    {
        html += "<div class='card'>";
        html += "<p style='color:#999;margin:0;'>No departures cached. Data appears after the first API fetch.</p>";
        html += "</div>";
    }
    else
    {
        html += "<div class='card' style='padding:0;overflow-x:auto;'>";
        html += "<table>";
        html += "<thead><tr>";
        html += "<th>#</th>";
        html += "<th>Line</th>";
        html += "<th>Destination</th>";
        html += "<th style='text-align:right;'>ETA</th>";
        html += "<th class='center'>2nd</th>";
        html += "<th class='center'>Plat</th>";
        html += "<th class='center'>Delay</th>";
        html += "<th class='center'>AC</th>";
        html += "<th>Stop ID</th>";
        html += "</tr></thead><tbody>";

        for (int i = 0; i < count; i++)
        {
            html += "<tr>";

            // Row number
            html += "<td style='color:#666;font-size:12px;'>" + String(i + 1) + "</td>";

            // Line
            html += "<td style='color:#67e8f9;font-weight:bold;'>";
            html += escapeHtml(deps[i].line);
            html += "</td>";

            // Destination
            html += "<td>";
            html += escapeHtml(deps[i].destination);
            html += "</td>";

            // ETA
            html += "<td style='text-align:right;color:#2ed573;font-weight:bold;'>";
            html += String(deps[i].eta) + " min";
            html += "</td>";

            // Second ETA
            html += "<td class='center' style='color:#999;font-size:12px;'>";
            if (deps[i].secondEta >= 0)
                html += String(deps[i].secondEta) + " min";
            else
                html += "-";
            html += "</td>";

            // Platform
            html += "<td class='center' style='color:#fcd34d;font-size:12px;'>";
            if (deps[i].platform[0] != '\0')
                html += escapeHtml(deps[i].platform);
            else
                html += "-";
            html += "</td>";

            // Delay
            html += "<td class='center' style='font-size:12px;'>";
            if (deps[i].isDelayed)
                html += "<span style='color:#fb7185;'>+" + String(deps[i].delayMinutes) + "</span>";
            else
                html += "<span style='color:#666;'>-</span>";
            html += "</td>";

            // AC
            html += "<td class='center' style='font-size:12px;'>";
            if (deps[i].hasAC)
                html += "<span style='color:#67e8f9;'>*</span>";
            else
                html += "<span style='color:#666;'>-</span>";
            html += "</td>";

            // Stop ID
            html += "<td style='color:#666;font-size:11px;'>";
            if (deps[i].sourceStopId[0] != '\0')
                html += escapeHtml(deps[i].sourceStopId);
            else
                html += "-";
            html += "</td>";

            html += "</tr>";
        }

        html += "</tbody></table>";
        html += "</div>";
    }

    html += "</div>"; // End dep-table

    // Back link at bottom
    html += "<div style='text-align:center;margin-top:24px;'>";
    html += "<a href='/' style='color:#67e8f9;text-decoration:none;'>&larr; Back to Dashboard</a>";
    html += "</div>";

    html += "</div>"; // End content

    // Inline script for AJAX refresh - builds table from JSON using safe DOM escaping
    html += "<script>"
            "function esc(s){if(!s)return '';var d=document.createElement('div');"
            "d.appendChild(document.createTextNode(s));return d.innerHTML;}"
            "function refreshDepartures(){"
            "var btn=document.getElementById('refreshBtn');"
            "btn.disabled=true;btn.textContent='Loading...';"
            "fetch('/departures-data').then(function(r){return r.json();}).then(function(d){"
            "document.getElementById('dep-count').textContent="
            "'Showing '+d.count+' cached departure'+(d.count!=1?'s':'')+' (max '+d.max+')';"
            "var c=document.getElementById('dep-table');"
            "c.textContent='';"
            "if(d.count==0){"
            "var p=document.createElement('div');p.className='card';"
            "p.textContent='No departures cached.';c.appendChild(p);"
            "}else{"
            "var w=document.createElement('div');w.className='card';"
            "w.style.padding='0';w.style.overflowX='auto';"
            "var t=document.createElement('table');"
            "var th=document.createElement('thead');"
            "var hr=document.createElement('tr');"
            "var hds=['#','Line','Destination','ETA','2nd','Plat','Delay','AC','Stop ID'];"
            "for(var h=0;h<hds.length;h++){"
            "var cell=document.createElement('th');cell.textContent=hds[h];"
            "if(h==3)cell.style.textAlign='right';"
            "if(h>=4)cell.className='center';"
            "hr.appendChild(cell);}"
            "th.appendChild(hr);t.appendChild(th);"
            "var tb=document.createElement('tbody');"
            "for(var i=0;i<d.deps.length;i++){var dep=d.deps[i];"
            "var r=document.createElement('tr');"
            "var vals=["
            "{v:''+(i+1),s:'color:#666;font-size:12px;'},"
            "{v:dep.l,s:'color:#67e8f9;font-weight:bold;'},"
            "{v:dep.d,s:''},"
            "{v:dep.e+' min',s:'text-align:right;color:#2ed573;font-weight:bold;'},"
            "{v:dep.s>=0?dep.s+' min':'-',s:'text-align:center;color:#999;font-size:12px;'},"
            "{v:dep.p||'-',s:'text-align:center;color:#fcd34d;font-size:12px;'},"
            "{v:dep.dl?'+'+dep.dm:'-',s:'text-align:center;font-size:12px;color:'+(dep.dl?'#fb7185':'#666')},"
            "{v:dep.ac?'*':'-',s:'text-align:center;font-size:12px;color:'+(dep.ac?'#67e8f9':'#666')},"
            "{v:dep.sid||'-',s:'color:#666;font-size:11px;'}"
            "];"
            "for(var j=0;j<vals.length;j++){"
            "var td=document.createElement('td');td.textContent=vals[j].v;"
            "if(vals[j].s)td.style.cssText=vals[j].s;"
            "r.appendChild(td);}"
            "tb.appendChild(r);}"
            "t.appendChild(tb);w.appendChild(t);c.appendChild(w);}"
            "btn.disabled=false;btn.textContent='Refresh from Memory';"
            "}).catch(function(e){"
            "btn.disabled=false;btn.textContent='Refresh from Memory';"
            "alert('Failed to fetch: '+e.message);"
            "});}"
            "</script>";

    html += FPSTR(HTML_FOOTER);
    return html;
}

String buildDeparturesJson(const Departure* deps, int count)
{
    String json = "{\"count\":";
    json += String(count);
    json += ",\"max\":";
    json += String(MAX_DEPARTURES);
    json += ",\"deps\":[";

    for (int i = 0; i < count; i++)
    {
        if (i > 0)
            json += ",";
        json += "{\"l\":\"";
        json += escapeJsonString(deps[i].line);
        json += "\",\"d\":\"";
        json += escapeJsonString(deps[i].destination);
        json += "\",\"e\":";
        json += String(deps[i].eta);
        json += ",\"s\":";
        json += String(deps[i].secondEta);
        json += ",\"p\":\"";
        json += escapeJsonString(deps[i].platform);
        json += "\",\"ac\":";
        json += deps[i].hasAC ? "true" : "false";
        json += ",\"dl\":";
        json += deps[i].isDelayed ? "true" : "false";
        json += ",\"dm\":";
        json += String(deps[i].delayMinutes);
        json += ",\"sid\":\"";
        json += escapeJsonString(deps[i].sourceStopId);
        json += "\"}";

    }

    json += "]}";
    return json;
}
