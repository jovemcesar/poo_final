import json


def test_formato_basico_da_leitura():
    dados = {
        "estacao": "EB-27",
        "ciclo": 1,
        "timestamp": "2026-01-01T10:00:00",
        "assinatura": {
            "id_dupla": 27,
            "nivel_baixo": 27.0,
            "nivel_alto": 84.0,
            "pressao_alta": 6.8,
        },
        "leituras": [
            {
                "tag": "LT-101",
                "tipo": "nivel",
                "valor": 50.0,
                "unidade": "%",
                "timestamp": "2026-01-01T10:00:00",
                "status": "OK",
            }
        ],
        "bombas": [
            {
                "tag": "P-101A",
                "ligada": False,
                "bloqueada": False,
                "horas": 0,
            }
        ],
        "alarmes": [],
    }

    texto_json = json.dumps(dados)
    dados_lidos = json.loads(texto_json)

    primeira_leitura = dados_lidos["leituras"][0]

    assert "tag" in primeira_leitura
    assert "valor" in primeira_leitura
    assert "unidade" in primeira_leitura
    assert "timestamp" in primeira_leitura
    assert "status" in primeira_leitura

    assert dados_lidos["assinatura"]["nivel_baixo"] == 27.0
    assert dados_lidos["assinatura"]["nivel_alto"] == 84.0
    assert dados_lidos["assinatura"]["pressao_alta"] == 6.8