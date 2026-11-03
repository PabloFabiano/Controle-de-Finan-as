import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts

ApplicationWindow {
    id: window
    width: 960
    height: 720
    minimumWidth: 680
    minimumHeight: 560
    visible: true
    title: qsTr("Controle financeiro mensal")
    color: "#f6f7fb"

    property date selectedMonth: new Date(new Date().getFullYear(), new Date().getMonth(), 1)
    property var entries: []
    property var summary: ({})
    readonly property string monthKey: Qt.formatDate(selectedMonth, "yyyy-MM")

    function monthTitle() {
        var text = Qt.formatDate(selectedMonth, "MMMM yyyy", Qt.locale("pt_BR"))
        return text.charAt(0).toUpperCase() + text.slice(1)
    }

    function refreshData() {
        entries = finance.entriesForMonth(monthKey)
        summary = finance.summaryForMonth(monthKey)
    }

    function previousMonth() {
        selectedMonth = new Date(selectedMonth.getFullYear(), selectedMonth.getMonth() - 1, 1)
    }

    function nextMonth() {
        selectedMonth = new Date(selectedMonth.getFullYear(), selectedMonth.getMonth() + 1, 1)
    }

    Component.onCompleted: refreshData()
    onSelectedMonthChanged: refreshData()

    Connections {
        target: finance
        function onDataChanged() { window.refreshData() }
    }

    header: ToolBar {
        background: Rectangle { color: "#17365d" }
        contentItem: RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 22
            anchors.rightMargin: 22
            spacing: 12

            Label {
                text: "Controle financeiro"
                color: "white"
                font.pixelSize: 21
                font.bold: true
                Layout.fillWidth: true
            }
            Label {
                text: "Dados salvos neste computador"
                color: "#d8e7fb"
                font.pixelSize: 13
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Frame {
            Layout.fillWidth: true
            padding: 10
            background: Rectangle { color: "white"; radius: 8; border.color: "#d8dee9" }

            RowLayout {
                anchors.fill: parent
                ToolButton {
                    text: "‹"
                    font.pixelSize: 26
                    onClicked: window.previousMonth()
                    ToolTip.visible: hovered
                    ToolTip.text: "Mês anterior"
                }
                Label {
                    text: window.monthTitle()
                    font.pixelSize: 19
                    font.bold: true
                    horizontalAlignment: Text.AlignHCenter
                    Layout.fillWidth: true
                }
                ToolButton {
                    text: "›"
                    font.pixelSize: 26
                    onClicked: window.nextMonth()
                    ToolTip.visible: hovered
                    ToolTip.text: "Próximo mês"
                }
            }
        }

        TabBar {
            id: tabs
            Layout.fillWidth: true
            TabButton { text: "Lançamentos" }
            TabButton { text: "Resumo do mês" }
        }

        StackLayout {
            currentIndex: tabs.currentIndex
            Layout.fillWidth: true
            Layout.fillHeight: true

            Item {
                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    Frame {
                        Layout.fillWidth: true
                        padding: 16
                        background: Rectangle { color: "white"; radius: 8; border.color: "#d8dee9" }

                        GridLayout {
                            id: form
                            anchors.fill: parent
                            columns: window.width < 760 ? 2 : 3
                            columnSpacing: 12
                            rowSpacing: 9

                            Label { text: "Data" }
                            Label { text: "Categoria" }
                            Label { text: "Descrição" }
                            TextField {
                                id: dateField
                                text: Qt.formatDate(new Date(), "dd/MM/yyyy")
                                placeholderText: "dd/MM/aaaa"
                                selectByMouse: true
                                Layout.fillWidth: true
                            }
                            ComboBox {
                                id: categoryBox
                                model: ["Geral", "Alimentação", "Água", "Roupas"]
                                Layout.fillWidth: true
                            }
                            TextField {
                                id: descriptionField
                                placeholderText: "Ex.: Mercado, camiseta, banho"
                                selectByMouse: true
                                Layout.fillWidth: true
                            }

                            Label { text: "Valor (R$)" }
                            Label { text: "Quantidade" }
                            Label { text: "Unidade" }
                            TextField {
                                id: amountField
                                placeholderText: "Ex.: 59,90"
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                selectByMouse: true
                                Layout.fillWidth: true
                            }
                            TextField {
                                id: quantityField
                                placeholderText: "Ex.: 2,5"
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                selectByMouse: true
                                Layout.fillWidth: true
                            }
                            ComboBox {
                                id: unitBox
                                editable: true
                                model: ["", "un", "g", "kg", "ml", "L", "m³"]
                                Layout.fillWidth: true
                            }

                            Label {
                                text: "Preencha valor, quantidade ou os dois."
                                color: "#64748b"
                                font.pixelSize: 12
                                Layout.columnSpan: form.columns - 1
                            }
                            Button {
                                text: "Salvar lançamento"
                                highlighted: true
                                Layout.alignment: Qt.AlignRight
                                onClicked: {
                                    if (finance.addEntry(dateField.text, categoryBox.currentText,
                                                         descriptionField.text, amountField.text,
                                                         quantityField.text, unitBox.currentText)) {
                                        descriptionField.clear()
                                        amountField.clear()
                                        quantityField.clear()
                                        unitBox.currentIndex = 0
                                    }
                                }
                            }
                        }
                    }

                    Label {
                        visible: finance.statusMessage.length > 0
                        text: finance.statusMessage
                        color: finance.statusMessage.indexOf("sucesso") >= 0 || finance.statusMessage.indexOf("removido") >= 0
                               ? "#16794a" : "#b42318"
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }

                    Frame {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        padding: 0
                        background: Rectangle { color: "white"; radius: 8; border.color: "#d8dee9" }

                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 8
                            Label {
                                text: entries.length + (entries.length === 1 ? " lançamento" : " lançamentos")
                                font.bold: true
                                font.pixelSize: 16
                            }
                            Label {
                                visible: entries.length === 0
                                text: "Nenhum lançamento em " + window.monthTitle() + "."
                                color: "#64748b"
                                Layout.fillWidth: true
                                Layout.alignment: Qt.AlignHCenter | Qt.AlignVCenter
                            }
                            ListView {
                                visible: entries.length > 0
                                model: entries
                                clip: true
                                spacing: 6
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                delegate: Rectangle {
                                    width: ListView.view.width
                                    height: 44
                                    color: "#f8fafc"
                                    radius: 5
                                    border.color: "#e2e8f0"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 9
                                        spacing: 10
                                        Label { text: modelData.dateDisplay; color: "#475569"; Layout.preferredWidth: 84 }
                                        Label { text: modelData.category; font.bold: true; Layout.preferredWidth: 90 }
                                        Label { text: modelData.description; elide: Text.ElideRight; Layout.fillWidth: true }
                                        Label { text: modelData.hasQuantity ? modelData.quantityDisplay + " " + modelData.unit : ""; Layout.preferredWidth: 95; horizontalAlignment: Text.AlignRight }
                                        Label { text: modelData.amountDisplay; font.bold: modelData.hasAmount; Layout.preferredWidth: 105; horizontalAlignment: Text.AlignRight }
                                        ToolButton {
                                            text: "Excluir"
                                            onClicked: finance.removeEntry(modelData.id)
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Item {
                Flickable {
                    anchors.fill: parent
                    contentWidth: width
                    contentHeight: summaryColumn.implicitHeight
                    clip: true

                    ColumnLayout {
                        id: summaryColumn
                        width: parent.width
                        spacing: 12

                        Frame {
                            Layout.fillWidth: true
                            padding: 18
                            background: Rectangle { color: "#17365d"; radius: 8 }
                            ColumnLayout {
                                anchors.fill: parent
                                Label { text: "Total gasto em " + window.monthTitle(); color: "#d8e7fb" }
                                Label { text: summary.totalAmountDisplay || "R$ 0,00"; color: "white"; font.pixelSize: 30; font.bold: true }
                                Label { text: (summary.entryCount || 0) + " lançamentos registrados"; color: "#d8e7fb" }
                            }
                        }

                        Frame {
                            Layout.fillWidth: true
                            padding: 14
                            background: Rectangle { color: "white"; radius: 8; border.color: "#d8dee9" }
                            ColumnLayout {
                                anchors.fill: parent
                                Label { text: "Gastos por categoria"; font.bold: true; font.pixelSize: 17 }
                                Repeater {
                                    model: summary.expensesByCategory || []
                                    delegate: RowLayout {
                                        Label { text: modelData.category; Layout.fillWidth: true }
                                        Label { text: modelData.amountDisplay; font.bold: true }
                                    }
                                }
                                Label { visible: (summary.expensesByCategory || []).length === 0; text: "Nenhum valor registrado."; color: "#64748b" }
                            }
                        }

                        Frame {
                            Layout.fillWidth: true
                            padding: 14
                            background: Rectangle { color: "white"; radius: 8; border.color: "#d8dee9" }
                            ColumnLayout {
                                anchors.fill: parent
                                Label { text: "Consumo de água"; font.bold: true; font.pixelSize: 17 }
                                Repeater {
                                    model: summary.waterByUnit || []
                                    delegate: RowLayout {
                                        Label { text: "Água"; Layout.fillWidth: true }
                                        Label { text: modelData.quantityDisplay + " " + modelData.unit; font.bold: true }
                                    }
                                }
                                Label { visible: (summary.waterByUnit || []).length === 0; text: "Nenhuma quantidade de água registrada."; color: "#64748b" }
                            }
                        }

                        Frame {
                            Layout.fillWidth: true
                            padding: 14
                            background: Rectangle { color: "white"; radius: 8; border.color: "#d8dee9" }
                            ColumnLayout {
                                anchors.fill: parent
                                Label { text: "Alimentação e outros consumos"; font.bold: true; font.pixelSize: 17 }
                                Repeater {
                                    model: summary.consumptionByCategory || []
                                    delegate: RowLayout {
                                        Label { text: modelData.category; Layout.fillWidth: true }
                                        Label { text: modelData.quantityDisplay + " " + modelData.unit; font.bold: true }
                                    }
                                }
                                Label { visible: (summary.consumptionByCategory || []).length === 0; text: "Nenhuma quantidade registrada."; color: "#64748b" }
                            }
                        }
                    }
                }
            }
        }
    }
}
