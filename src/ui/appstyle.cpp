#include "ui/appstyle.h"

QString applicationStyleSheet()
{
    return QStringLiteral(R"QSS(
        * {
            font-family: "Microsoft YaHei UI", "Microsoft YaHei", sans-serif;
            font-size: 14px;
            color: #26382b;
        }

        QMainWindow,
        QWidget#appRoot,
        QWidget[page="true"] {
            background: #eaf5e9;
        }

        QWidget#authPage {
            background: #e5f3e6;
        }

        QFrame#authCard {
            background: white;
            border: 1px solid #cfe4d1;
            border-radius: 18px;
        }

        QLabel#authLogo {
            color: white;
            background: #3da34a;
            border-radius: 34px;
            font-size: 34px;
            min-width: 68px;
            max-width: 68px;
            min-height: 68px;
            max-height: 68px;
        }

        QLabel#authTitle {
            color: #176a2c;
            font-size: 27px;
            font-weight: 700;
        }

        QFrame#appHeader {
            background: #287d32;
            border: none;
            border-radius: 12px;
        }

        QLabel#brandTitle {
            color: white;
            font-size: 24px;
            font-weight: 700;
        }

        QLabel#brandSubtitle {
            color: #d9f2dd;
            font-size: 12px;
        }

        QLabel#headerAccountBadge {
            color: white;
            background: #176526;
            border: 1px solid #4a9c55;
            border-radius: 9px;
            padding: 9px 16px;
            font-weight: 600;
        }

        QFrame[card="true"] {
            background: white;
            border: 1px solid #dcebdd;
            border-radius: 12px;
        }

        QFrame#profileSidebar {
            background: #fbfefb;
        }

        QLabel#avatarCircle {
            color: white;
            background: #3fa64b;
            border: 5px solid #dff1e1;
            border-radius: 41px;
            font-size: 30px;
            font-weight: 700;
        }

        QLabel#profileName {
            color: #1e6e2d;
            font-size: 22px;
            font-weight: 700;
        }

        QFrame#metricCard {
            background: #f3f7f3;
            border: none;
            border-radius: 10px;
        }

        QLabel[role="metricCaption"] {
            color: #516856;
            font-weight: 700;
        }

        QLabel[role="metricValue"] {
            color: #27823a;
            font-size: 27px;
            font-weight: 700;
        }

        QLabel[role="infoBlock"] {
            background: #f4f7f4;
            border-radius: 9px;
            padding: 12px;
            line-height: 1.5;
        }

        QLabel[role="goalBlock"] {
            background: #eff8ef;
            border: 1px solid #67b96e;
            border-radius: 9px;
            padding: 12px;
            line-height: 1.5;
        }

        QLabel[role="sectionTitle"] {
            color: #285e31;
            font-size: 17px;
            font-weight: 700;
        }

        QLabel[recommendation="exercise"] {
            background: #f5f8f5;
            border: 1px solid #d7e4d8;
            border-left: 5px solid #3fa64b;
            border-radius: 9px;
            padding: 14px;
            line-height: 1.5;
        }

        QLabel[recommendation="meal"] {
            background: #fff9e8;
            border: 1px solid #f2ce66;
            border-left: 5px solid #f0a525;
            border-radius: 9px;
            padding: 14px;
            line-height: 1.5;
        }

        QLabel[role="summaryBlock"] {
            background: #eef7ef;
            color: #2d6635;
            border-radius: 8px;
            padding: 11px;
            font-weight: 600;
        }

        QLabel[role="pageTitle"] {
            color: #176a2c;
            font-size: 21px;
            font-weight: 700;
            padding: 2px 0;
        }

        QLabel[role="subtitle"] {
            color: #6d7f70;
            font-size: 13px;
        }

        QLabel[role="currentUser"] {
            background: #e5f4e7;
            color: #206e2f;
            border: 1px solid #b9ddbd;
            border-radius: 8px;
            padding: 8px 12px;
            font-weight: 600;
        }

        QLabel[role="resultCard"] {
            background: #f7fbf7;
            border: 1px solid #badfbe;
            border-left: 5px solid #43a047;
            border-radius: 9px;
            padding: 14px;
            color: #2d4532;
            line-height: 1.5;
        }

        QTabWidget::pane {
            border: none;
            background: transparent;
            top: -1px;
        }

        QTabBar::tab {
            background: #f8fcf8;
            color: #4e6652;
            border: 1px solid #d7e8d9;
            border-bottom: none;
            padding: 11px 24px;
            margin-right: 4px;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            min-width: 92px;
        }

        QTabBar::tab:hover {
            background: #e3f3e5;
            color: #227332;
        }

        QTabBar::tab:selected {
            background: #31853c;
            color: white;
            border-color: #31853c;
            font-weight: 700;
        }

        QLineEdit,
        QComboBox,
        QSpinBox,
        QDoubleSpinBox {
            background: #fbfdfb;
            border: 1px solid #cadbcb;
            border-radius: 7px;
            padding: 7px 9px;
            min-height: 22px;
            selection-background-color: #65b96d;
        }

        QLineEdit:focus,
        QComboBox:focus,
        QSpinBox:focus,
        QDoubleSpinBox:focus {
            background: white;
            border: 2px solid #45a653;
        }

        QComboBox::drop-down {
            border: none;
            width: 26px;
        }

        QPushButton {
            background: #edf5ee;
            color: #285f31;
            border: 1px solid #c6dec9;
            border-radius: 8px;
            padding: 9px 18px;
            font-weight: 600;
            min-height: 20px;
        }

        QPushButton:hover {
            background: #dceddf;
            border-color: #82bd89;
        }

        QPushButton:pressed {
            background: #cae3ce;
        }

        QPushButton[compact="true"] {
            padding: 6px 9px;
            min-height: 18px;
            font-size: 12px;
        }

        QPushButton[variant="primary"] {
            background: #43a047;
            color: white;
            border-color: #43a047;
        }

        QPushButton[variant="primary"]:hover {
            background: #348c3a;
        }

        QPushButton[variant="warning"] {
            background: #f57c00;
            color: white;
            border-color: #f57c00;
        }

        QPushButton[variant="warning"]:hover {
            background: #df6e00;
        }

        QPushButton[variant="danger"] {
            background: #fff0f0;
            color: #c43b3b;
            border-color: #efb8b8;
        }

        QPushButton[variant="danger"]:hover {
            background: #ffe1e1;
            border-color: #de8585;
        }

        QTableWidget {
            background: white;
            alternate-background-color: #f4faf4;
            border: 1px solid #d9e8da;
            border-radius: 10px;
            gridline-color: #e5eee6;
            selection-background-color: #d7eed9;
            selection-color: #183d20;
        }

        QTableWidget::item {
            padding: 7px;
            border-bottom: 1px solid #edf3ed;
        }

        QHeaderView::section {
            background: #e2f1e4;
            color: #226d31;
            border: none;
            border-right: 1px solid #cfdfd1;
            border-bottom: 1px solid #bdd6c0;
            padding: 9px 6px;
            font-weight: 700;
        }

        QScrollBar:vertical {
            background: #eff6ef;
            width: 10px;
            margin: 0;
        }

        QScrollBar::handle:vertical {
            background: #a6cba9;
            border-radius: 5px;
            min-height: 28px;
        }

        QStatusBar {
            background: #e1eee2;
            color: #607463;
            border-top: 1px solid #d0dfd1;
        }

        QMessageBox {
            background: #f4faf4;
        }
    )QSS");
}
