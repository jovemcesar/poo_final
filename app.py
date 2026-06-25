import json
from pathlib import Path
from datetime import datetime

import pandas as pd
import streamlit as st


BASE_DIR = Path(__file__).resolve().parents[1]

ARQ_LEITURAS = BASE_DIR / "data" / "leituras.jsonl"
ARQ_HISTORICO = BASE_DIR / "data" / "historico.csv"
ARQ_COMANDOS = BASE_DIR / "data" / "comandos.jsonl"


st.set_page_config(page_title="Mini-SCADA EB-27", layout="wide")

st.title("Mini-SCADA - EB-27")
st.caption("Tela para acompanhar as leituras e o estado das bombas.")


def ler_leituras():
    if not ARQ_LEITURAS.exists():
        return []

    dados = []

    with ARQ_LEITURAS.open("r", encoding="utf-8") as arquivo:
        for linha in arquivo:
            linha = linha.strip()

            if linha == "":
                continue

            try:
                dados.append(json.loads(linha))
            except json.JSONDecodeError:
                continue

    return dados


def criar_tabela(pacotes):
    linhas = []

    for pacote in pacotes:
        leituras = pacote.get("leituras", [])

        for item in leituras:
            linhas.append({
                "estacao": pacote.get("estacao"),
                "ciclo": pacote.get("ciclo"),
                "timestamp": pacote.get("timestamp"),
                "tag": item.get("tag"),
                "tipo": item.get("tipo"),
                "valor": item.get("valor"),
                "unidade": item.get("unidade"),
                "status": item.get("status"),
            })

    return pd.DataFrame(linhas)


def salvar_csv(tabela):
    if tabela.empty:
        return

    ARQ_HISTORICO.parent.mkdir(parents=True, exist_ok=True)
    tabela.to_csv(ARQ_HISTORICO, index=False, encoding="utf-8")


def salvar_comando(comando, obs=""):
    ARQ_COMANDOS.parent.mkdir(parents=True, exist_ok=True)

    registro = {
        "timestamp": datetime.now().isoformat(timespec="seconds"),
        "comando": comando,
        "origem": "tela_supervisorio",
        "observacao": obs,
        "status": "registrado"
    }

    with ARQ_COMANDOS.open("a", encoding="utf-8") as arquivo:
        arquivo.write(json.dumps(registro, ensure_ascii=False) + "\n")

    return registro


pacotes = ler_leituras()
df = criar_tabela(pacotes)

salvar_csv(df)

if len(pacotes) == 0:
    st.warning("Nenhuma leitura encontrada. Rode primeiro o programa em C++.")
    st.stop()


ultimo_pacote = pacotes[-1]
config = ultimo_pacote.get("assinatura", {})


col1, col2, col3, col4 = st.columns(4)

col1.metric("Estação", ultimo_pacote.get("estacao", "-"))
col2.metric("Ciclo", ultimo_pacote.get("ciclo", 0))
col3.metric("Nível baixo", f"{config.get('nivel_baixo', 0)} %")
col4.metric("Pressão alta", f"{config.get('pressao_alta', 0)} bar")


st.subheader("Leituras")

tabela_leituras = pd.DataFrame(ultimo_pacote.get("leituras", []))
st.dataframe(tabela_leituras, use_container_width=True)


st.subheader("Bombas")

tabela_bombas = pd.DataFrame(ultimo_pacote.get("bombas", []))
st.dataframe(tabela_bombas, use_container_width=True)


st.subheader("Histórico")

if not df.empty:
    grafico = df.pivot_table(
        index="ciclo",
        columns="tipo",
        values="valor",
        aggfunc="last"
    )

    colunas = []

    for nome in ["nivel", "pressao", "vazao", "temperatura_motor"]:
        if nome in grafico.columns:
            colunas.append(nome)

    if len(colunas) > 0:
        st.line_chart(grafico[colunas])

    st.dataframe(df.tail(40), use_container_width=True)


st.subheader("Alarmes")

tabela_alarmes = pd.DataFrame(ultimo_pacote.get("alarmes", []))

if tabela_alarmes.empty:
    st.success("Nenhum alarme no momento.")
else:
    st.dataframe(tabela_alarmes, use_container_width=True)


st.subheader("Comandos manuais")

btn1, btn2, btn3, btn4 = st.columns(4)

if btn1.button("Ligar bomba A"):
    salvar_comando("LIGAR_BOMBA_A", "Comando enviado pela tela.")
    st.success("Comando registrado.")

if btn2.button("Desligar bomba A"):
    salvar_comando("DESLIGAR_BOMBA_A", "Comando enviado pela tela.")
    st.success("Comando registrado.")

if btn3.button("Ligar bomba B"):
    salvar_comando("LIGAR_BOMBA_B", "Comando enviado pela tela.")
    st.success("Comando registrado.")

if btn4.button("Resetar bloqueios"):
    salvar_comando("RESETAR_BLOQUEIOS", "Reset solicitado pelo operador.")
    st.success("Comando registrado.")


if ARQ_COMANDOS.exists():
    lista_comandos = []

    for linha in ARQ_COMANDOS.read_text(encoding="utf-8").splitlines():
        try:
            lista_comandos.append(json.loads(linha))
        except json.JSONDecodeError:
            continue

    if len(lista_comandos) > 0:
        st.subheader("Últimos comandos")
        st.dataframe(pd.DataFrame(lista_comandos).tail(20), use_container_width=True)


st.info("Teste considerado: sensor de nível travado por 10 ciclos e bloqueio de partida com motor em alerta.")