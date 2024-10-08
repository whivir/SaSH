﻿#include "stdafx.h"
#include "customtitlebar.h"

CustomTitleBar::CustomTitleBar(DWORD button, QWidget* parent)
	: QWidget(parent), parent_(parent)
{
	setAttribute(Qt::WA_StyledBackground);
	setFixedHeight(35);

	QString style = R"(

QWidget { background-color: rgb(31, 31, 31); }

QLabel {
	color: rgb(214, 214, 214);
	font: 12pt "Consolas";
}

QPushButton {
	background-color: rgb(31, 31, 31);
	border: 0px solid #dcdfe6;
	color: rgb(214, 214, 214);
	padding: 3px;
}

QPushButton:hover {
	background-color: rgb(61, 61, 61);
}

QPushButton:pressed, QPushButton:checked {
	background-color: rgb(56, 56, 56);
}
		)";

	setStyleSheet(style);

	QLabel* icon = q_check_ptr(new QLabel(""));
	sash_assume(icon != nullptr);
	icon->setFixedSize(35, 35);
	QPixmap originalPixmap(":/image/ico.png");
	QPixmap scaledPixmap = originalPixmap.scaled(QSize(25, 25), Qt::KeepAspectRatio, Qt::SmoothTransformation);
	icon->setPixmap(scaledPixmap);

	titleLabel_ = q_check_ptr(new QLabel("sash"));
	sash_assume(titleLabel_ != nullptr);
	titleLabel_->setFixedHeight(35);
	titleLabel_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

	QPushButton* minimizeButton = nullptr;
	if (button & kMinimizeButton)
	{
		minimizeButton = q_check_ptr(new QPushButton(""));
		sash_assume(minimizeButton != nullptr);
		if (minimizeButton != nullptr)
		{
			minimizeButton->setFixedSize(35, 35);
			minimizeButton->setIcon(QIcon(":/image/icon_min"));
			connect(minimizeButton, &QPushButton::clicked, parent, &QMainWindow::showMinimized);
		}
	}

	if (button & kMaximizeButton)
	{
		maximizeButton_ = q_check_ptr(new QPushButton(""));
		sash_assume(maximizeButton_ != nullptr);
		if (maximizeButton_ != nullptr)
		{
			maximizeButton_->setFixedSize(35, 35);

			if (!parent_->isMaximized())
				maximizeButton_->setIcon(QIcon(":/image/icon_max.png"));
			else
				maximizeButton_->setIcon(QIcon(":/image/icon_restoredown.png"));
			connect(maximizeButton_, &QPushButton::clicked, this, &CustomTitleBar::toggleMaximize);
		}
	}

	QPushButton* closeButton = nullptr;
	if (button & kCloseButton)
	{
		closeButton = q_check_ptr(new QPushButton(""));
		sash_assume(closeButton != nullptr);
		if (closeButton != nullptr)
		{
			closeButton->setFixedSize(35, 35);
			closeButton->setIcon(QIcon(":/image/icon_close"));
			connect(closeButton, &QPushButton::clicked, parent, &QMainWindow::close);
		}
	}

	QHBoxLayout* layout = q_check_ptr(new QHBoxLayout(this));
	sash_assume(layout != nullptr);
	if (layout == nullptr)
		return;

	layout->setContentsMargins(5, 0, 0, 0);
	layout->setSpacing(0);
	layout->addWidget(icon);
	if (titleLabel_ != nullptr)
		layout->addWidget(titleLabel_);
	if (minimizeButton != nullptr)
		layout->addWidget(minimizeButton);
	if (maximizeButton_ != nullptr)
		layout->addWidget(maximizeButton_);
	if (closeButton != nullptr)
		layout->addWidget(closeButton);

	connect(parent, &QMainWindow::windowTitleChanged, this, &CustomTitleBar::onTitleChanged);
}

CustomTitleBar::~CustomTitleBar()
{

}

void CustomTitleBar::onTitleChanged(const QString& title)
{
	if (titleLabel_ != nullptr)
	{
		titleLabel_->setText(title);
	}
}

void CustomTitleBar::mouseDoubleClickEvent(QMouseEvent* e)
{
	std::ignore = e;
	if (maximizeButton_ == nullptr)
		return;
	toggleMaximize();
}

void CustomTitleBar::mousePressEvent(QMouseEvent* e)
{
	if (e->button() == Qt::LeftButton)
	{
		dragStartPosition_ = e->pos();
	}
}

void CustomTitleBar::mouseMoveEvent(QMouseEvent* e)
{
	if (e->buttons() & Qt::LeftButton)
	{
		if ((e->pos() - dragStartPosition_).manhattanLength() >= QApplication::startDragDistance())
		{
			QWidget* parentWindow = qobject_cast<QWidget*>(parent());
			if (parentWindow && parentWindow->isMaximized())
			{
				parentWindow->showNormal();
			}

			QPoint diff = e->pos() - dragStartPosition_;
			QPoint newpos = parentWidget()->pos() + diff;
			parentWidget()->move(newpos);
		}
	}
}

void CustomTitleBar::mouseReleaseEvent(QMouseEvent* e)
{
	std::ignore = e;
	dragStartPosition_ = QPoint();
}

void CustomTitleBar::toggleMaximize()
{
	if (maximizeButton_ == nullptr)
		return;

	if (parent_->isMaximized())
	{
		parent_->showNormal();
		maximizeButton_->setIcon(QIcon(":/image/icon_max.png"));
	}
	else
	{
		parent_->showMaximized();
		maximizeButton_->setIcon(QIcon(":/image/icon_restoredown.png"));
	}
}