import os
import pandas as pd
import streamlit as st
import plotly.express as px

from live_monitor import LiveMonitor


# ============================================================
# Page configuration
# ============================================================

st.set_page_config(
    page_title="C System Monitor Ver.4",
    page_icon="💻",
    layout="wide"
)


# ============================================================
# Paths
# ============================================================

SYSTEM_LOG = "logs/system_monitor.csv"
PROCESS_LOG = "logs/process_ranking.csv"
ALERT_LOG = "logs/alert_log.csv"


# ============================================================
# Live Monitor
# ============================================================

@st.cache_resource
def get_live_monitor():

    monitor = LiveMonitor(
        max_history=300
    )

    monitor.start()

    return monitor


live_monitor = get_live_monitor()


# ============================================================
# Utility functions
# ============================================================

def get_status(value, warning=70.0, critical=90.0):

    if value >= critical:
        return "CRITICAL"

    if value >= warning:
        return "WARNING"

    return "NORMAL"


def status_icon(status):

    if status == "CRITICAL":
        return "🔴"

    if status == "WARNING":
        return "🟡"

    return "🟢"


def load_system_log():

    if not os.path.exists(SYSTEM_LOG):
        return pd.DataFrame(
            columns=[
                "timestamp",
                "cpu_usage",
                "memory_usage",
                "disk_usage",
                "network_rx_kbps",
                "network_tx_kbps"
            ]
        )

    try:

        df = pd.read_csv(
            SYSTEM_LOG
        )

        expected_columns = [
            "timestamp",
            "cpu_usage",
            "memory_usage",
            "disk_usage",
            "network_rx_kbps",
            "network_tx_kbps"
        ]

        # Support old CSV format.
        if len(df.columns) == 4:

            df.columns = [
                "timestamp",
                "cpu_usage",
                "memory_usage",
                "disk_usage"
            ]

            df["network_rx_kbps"] = 0.0
            df["network_tx_kbps"] = 0.0

        elif len(df.columns) >= 6:

            df = df.iloc[:, :6]
            df.columns = expected_columns

        else:

            return pd.DataFrame(
                columns=expected_columns
            )

        df["timestamp"] = pd.to_datetime(
            df["timestamp"],
            errors="coerce"
        )

        numeric_columns = [
            "cpu_usage",
            "memory_usage",
            "disk_usage",
            "network_rx_kbps",
            "network_tx_kbps"
        ]

        for column in numeric_columns:

            df[column] = pd.to_numeric(
                df[column],
                errors="coerce"
            )

        df = df.dropna(
            subset=["timestamp"]
        )

        return df

    except Exception as e:

        st.warning(
            f"Failed to read system log: {e}"
        )

        return pd.DataFrame(
            columns=[
                "timestamp",
                "cpu_usage",
                "memory_usage",
                "disk_usage",
                "network_rx_kbps",
                "network_tx_kbps"
            ]
        )


def load_process_log():

    if not os.path.exists(PROCESS_LOG):
        return pd.DataFrame()

    try:

        df = pd.read_csv(
            PROCESS_LOG
        )

        if "timestamp" in df.columns:

            df["timestamp"] = pd.to_datetime(
                df["timestamp"],
                errors="coerce"
            )

        if "memory_mb" in df.columns:

            df["memory_mb"] = pd.to_numeric(
                df["memory_mb"],
                errors="coerce"
            )

        return df

    except Exception as e:

        st.warning(
            f"Failed to read process log: {e}"
        )

        return pd.DataFrame()


def load_alert_log():

    if not os.path.exists(ALERT_LOG):
        return pd.DataFrame()

    try:

        return pd.read_csv(
            ALERT_LOG
        )

    except Exception:

        return pd.DataFrame()


# ============================================================
# Header
# ============================================================

st.title(
    "💻 C System Monitor Ver.4"
)

st.caption(
    "Windows API + C + Winsock + Python + Pandas + Streamlit"
)


# ============================================================
# Live monitoring section
# ============================================================

@st.fragment(run_every="10s")
def live_monitor_section():

    socket_data = live_monitor.get_data()

    st.subheader(
        "🔌 Live System Monitor"
    )


    # --------------------------------------------------------
    # No live data
    # --------------------------------------------------------

    if not socket_data:

        st.warning(
            "Waiting for data from C System Monitor..."
        )

        st.info(
            "Make sure monitor.exe is running "
            "and listening on 127.0.0.1:5000."
        )

        return


    socket_df = pd.DataFrame(
        socket_data
    )

    socket_df["timestamp"] = pd.to_datetime(
        socket_df["timestamp"],
        errors="coerce"
    )


    latest = socket_df.iloc[-1]


    cpu = float(
        latest["cpu"]
    )

    memory = float(
        latest["memory"]
    )

    disk = float(
        latest["disk"]
    )

    network_rx = float(
        latest.get(
            "network_rx_kbps",
            0.0
        )
    )

    network_tx = float(
        latest.get(
            "network_tx_kbps",
            0.0
        )
    )


    # --------------------------------------------------------
    # Status
    # --------------------------------------------------------

    cpu_status = get_status(cpu)
    memory_status = get_status(memory)
    disk_status = get_status(disk)


    # --------------------------------------------------------
    # Metrics
    # --------------------------------------------------------

    col1, col2, col3, col4, col5 = st.columns(5)


    with col1:

        st.metric(
            "CPU",
            f"{cpu:.2f} %",
            delta=cpu_status,
            delta_color="normal" if cpu_status == "NORMAL" else "inverse"
        )


    with col2:

        st.metric(
            "Memory",
            f"{memory:.2f} %",
            delta=memory_status,
            delta_color="normal" if memory_status == "NORMAL" else "inverse"
        )


    with col3:

        st.metric(
            "Disk",
            f"{disk:.2f} %",
            delta=disk_status,
            delta_color="normal" if disk_status == "NORMAL" else "inverse"
        )


    with col4:

        st.metric(
            "Network RX",
            f"{network_rx:.2f} KB/s"
        )


    with col5:

        st.metric(
            "Network TX",
            f"{network_tx:.2f} KB/s"
        )


    st.caption(
        f"Last update: {latest['timestamp']}"
    )


    # --------------------------------------------------------
    # System status
    # --------------------------------------------------------

    st.subheader(
        "🚦 System Status"
    )


    status_columns = st.columns(3)


    with status_columns[0]:

        st.write(
            f"{status_icon(cpu_status)} "
            f"CPU: **{cpu_status}**"
        )


    with status_columns[1]:

        st.write(
            f"{status_icon(memory_status)} "
            f"Memory: **{memory_status}**"
        )


    with status_columns[2]:

        st.write(
            f"{status_icon(disk_status)} "
            f"Disk: **{disk_status}**"
        )


    # --------------------------------------------------------
    # Average values
    # --------------------------------------------------------

    st.subheader(
        "📊 Average Usage"
    )


    average_columns = st.columns(5)


    with average_columns[0]:

        st.metric(
            "CPU Average",
            f"{socket_df['cpu'].mean():.2f} %"
        )


    with average_columns[1]:

        st.metric(
            "Memory Average",
            f"{socket_df['memory'].mean():.2f} %"
        )


    with average_columns[2]:

        st.metric(
            "Disk Average",
            f"{socket_df['disk'].mean():.2f} %"
        )


    with average_columns[3]:

        st.metric(
            "RX Average",
            f"{socket_df['network_rx_kbps'].mean():.2f} KB/s"
        )


    with average_columns[4]:

        st.metric(
            "TX Average",
            f"{socket_df['network_tx_kbps'].mean():.2f} KB/s"
        )


    # --------------------------------------------------------
    # Live charts
    # --------------------------------------------------------

    st.subheader(
        "📈 Real-time Resource Usage"
    )


    chart_df = socket_df[
        [
            "timestamp",
            "cpu",
            "memory",
            "disk"
        ]
    ].melt(
        id_vars="timestamp",
        var_name="metric",
        value_name="usage"
    )


    chart_df["metric"] = chart_df[
        "metric"
    ].map(
        {
            "cpu": "CPU",
            "memory": "Memory",
            "disk": "Disk"
        }
    )


    fig = px.line(
        chart_df,
        x="timestamp",
        y="usage",
        color="metric",
        markers=True,
        title="CPU / Memory / Disk"
    )


    fig.update_layout(
        yaxis_title="Usage (%)",
        xaxis_title="Time",
        hovermode="x unified"
    )


    st.plotly_chart(
        fig,
        use_container_width=True
    )


    # --------------------------------------------------------
    # Network chart
    # --------------------------------------------------------

    network_df = socket_df[
        [
            "timestamp",
            "network_rx_kbps",
            "network_tx_kbps"
        ]
    ].melt(
        id_vars="timestamp",
        var_name="metric",
        value_name="speed"
    )


    network_df["metric"] = network_df[
        "metric"
    ].map(
        {
            "network_rx_kbps": "RX",
            "network_tx_kbps": "TX"
        }
    )


    network_fig = px.line(
        network_df,
        x="timestamp",
        y="speed",
        color="metric",
        markers=True,
        title="Network Traffic"
    )


    network_fig.update_layout(
        yaxis_title="KB/s",
        xaxis_title="Time",
        hovermode="x unified"
    )


    st.plotly_chart(
        network_fig,
        use_container_width=True
    )


    # --------------------------------------------------------
    # Live data table
    # --------------------------------------------------------

    with st.expander(
        "🔎 Live Socket Data"
    ):

        display_df = socket_df.copy()

        display_df = display_df.rename(
            columns={
                "timestamp": "Timestamp",
                "cpu": "CPU (%)",
                "memory": "Memory (%)",
                "disk": "Disk (%)",
                "network_rx_kbps": "Network RX (KB/s)",
                "network_tx_kbps": "Network TX (KB/s)"
            }
        )

        st.dataframe(
            display_df,
            use_container_width=True
        )


live_monitor_section()


# ============================================================
# Historical CSV data
# ============================================================

st.divider()

st.subheader(
    "📁 Historical System Logs"
)

system_df = load_system_log()


if system_df.empty:

    st.info(
        "No historical system log data available."
    )

else:

    historical_columns = st.columns(3)

    with historical_columns[0]:

        st.metric(
            "Records",
            len(system_df)
        )

    with historical_columns[1]:

        st.metric(
            "CPU Average",
            f"{system_df['cpu_usage'].mean():.2f} %"
        )

    with historical_columns[2]:

        st.metric(
            "Memory Average",
            f"{system_df['memory_usage'].mean():.2f} %"
        )


    with st.expander(
        "📄 System Log CSV"
    ):

        st.dataframe(
            system_df.tail(100),
            use_container_width=True
        )


# ============================================================
# Process ranking
# ============================================================

st.divider()

st.subheader(
    "🔥 Process Memory Ranking"
)

process_df = load_process_log()


if process_df.empty:

    st.info(
        "No process ranking data available yet."
    )

else:

    latest_process_time = process_df[
        "timestamp"
    ].max()


    latest_process_df = process_df[
        process_df["timestamp"] ==
        latest_process_time
    ].copy()


    latest_process_df = latest_process_df.sort_values(
        "rank"
    )


    st.dataframe(
        latest_process_df[
            [
                "rank",
                "pid",
                "process_name",
                "memory_mb"
            ]
        ],
        use_container_width=True,
        hide_index=True
    )


    # --------------------------------------------------------
    # Process chart
    # --------------------------------------------------------

    process_chart = latest_process_df.sort_values(
        "memory_mb",
        ascending=True
    )


    fig_process = px.bar(
        process_chart,
        x="memory_mb",
        y="process_name",
        orientation="h",
        title="Top Processes by Memory Usage"
    )


    fig_process.update_layout(
        xaxis_title="Memory (MB)",
        yaxis_title="Process"
    )


    st.plotly_chart(
        fig_process,
        use_container_width=True
    )


    # --------------------------------------------------------
    # Process history
    # --------------------------------------------------------

    with st.expander(
        "📈 Process Ranking History"
    ):

        st.dataframe(
            process_df.tail(100),
            use_container_width=True
        )


# ============================================================
# Alert history
# ============================================================

st.divider()

st.subheader(
    "🚨 Alert History"
)

alert_df = load_alert_log()


if alert_df.empty:

    st.success(
        "No alert history available."
    )

else:

    st.dataframe(
        alert_df.tail(100),
        use_container_width=True
    )


# ============================================================
# Footer
# ============================================================

st.divider()

st.caption(
    "C System Monitor Ver.4 | "
    "Windows API → C → TCP/NDJSON → Python → Streamlit"
)
# streamlit run python/dashboard.py
