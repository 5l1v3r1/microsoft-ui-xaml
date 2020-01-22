﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "common.h"
#include "TabView.h"
#include "TabViewItem.h"
#include "TabViewAutomationPeer.h"
#include "DoubleUtil.h"
#include "RuntimeProfiler.h"
#include "ResourceAccessor.h"
#include "SharedHelpers.h"
#include <Vector.h>

static constexpr double c_tabMinimumWidth = 48.0;
static constexpr double c_tabMaximumWidth = 200.0;

static constexpr wstring_view c_tabViewItemMinWidthName{ L"TabViewItemMinWidth"sv };
static constexpr wstring_view c_tabViewItemMaxWidthName{ L"TabViewItemMaxWidth"sv };

// TODO: what is the right number and should this be customizable?
static constexpr double c_scrollAmount = 50.0;

TabView::TabView()
{
    __RP_Marker_ClassById(RuntimeProfiler::ProfId_TabView);

    auto items = winrt::make<Vector<winrt::IInspectable, MakeVectorParam<VectorFlag::Observable>()>>();
    SetValue(s_TabItemsProperty, items);

    SetDefaultStyleKey(this);

    m_selectionModel.SingleSelect(true);

    Loaded({ this, &TabView::OnLoaded });

    // KeyboardAccelerator is only available on RS3+
    if (SharedHelpers::IsRS3OrHigher())
    {
        winrt::KeyboardAccelerator ctrlf4Accel;
        ctrlf4Accel.Key(winrt::VirtualKey::F4);
        ctrlf4Accel.Modifiers(winrt::VirtualKeyModifiers::Control);
        ctrlf4Accel.Invoked({ this, &TabView::OnCtrlF4Invoked });
        ctrlf4Accel.ScopeOwner(*this);
        KeyboardAccelerators().Append(ctrlf4Accel);
    }

    // Ctrl+Tab as a KeyboardAccelerator only works on 19H1+
    if (SharedHelpers::Is19H1OrHigher())
    {
        winrt::KeyboardAccelerator ctrlTabAccel;
        ctrlTabAccel.Key(winrt::VirtualKey::Tab);
        ctrlTabAccel.Modifiers(winrt::VirtualKeyModifiers::Control);
        ctrlTabAccel.Invoked({ this, &TabView::OnCtrlTabInvoked });
        ctrlTabAccel.ScopeOwner(*this);
        KeyboardAccelerators().Append(ctrlTabAccel);

        winrt::KeyboardAccelerator ctrlShiftTabAccel;
        ctrlShiftTabAccel.Key(winrt::VirtualKey::Tab);
        ctrlShiftTabAccel.Modifiers(winrt::VirtualKeyModifiers::Control | winrt::VirtualKeyModifiers::Shift);
        ctrlShiftTabAccel.Invoked({ this, &TabView::OnCtrlShiftTabInvoked });
        ctrlShiftTabAccel.ScopeOwner(*this);
        KeyboardAccelerators().Append(ctrlShiftTabAccel);
    }
}

void TabView::OnApplyTemplate()
{
    winrt::IControlProtected controlProtected{ *this };

    m_tabContentPresenter.set(GetTemplateChildT<winrt::ContentPresenter>(L"TabContentPresenter", controlProtected));
    m_rightContentPresenter.set(GetTemplateChildT<winrt::ContentPresenter>(L"RightContentPresenter", controlProtected));
    
    m_leftContentColumn.set(GetTemplateChildT<winrt::ColumnDefinition>(L"LeftContentColumn", controlProtected));
    m_tabColumn.set(GetTemplateChildT<winrt::ColumnDefinition>(L"TabColumn", controlProtected));
    m_addButtonColumn.set(GetTemplateChildT<winrt::ColumnDefinition>(L"AddButtonColumn", controlProtected));
    m_rightContentColumn.set(GetTemplateChildT<winrt::ColumnDefinition>(L"RightContentColumn", controlProtected));

    m_tabContainerGrid.set(GetTemplateChildT<winrt::Grid>(L"TabContainerGrid", controlProtected));

    m_shadowReceiver.set(GetTemplateChildT<winrt::Grid>(L"ShadowReceiver", controlProtected));

    /*### m_listView.set([this, controlProtected]() {
        auto listView = GetTemplateChildT<winrt::ListView>(L"TabListView", controlProtected);
        if (listView)
        {
            m_listViewLoadedRevoker = listView.Loaded(winrt::auto_revoke, { this, &TabView::OnListViewLoaded });
            m_listViewSelectionChangedRevoker = listView.SelectionChanged(winrt::auto_revoke, { this, &TabView::OnListViewSelectionChanged });

            m_listViewDragItemsStartingRevoker = listView.DragItemsStarting(winrt::auto_revoke, { this, &TabView::OnListViewDragItemsStarting });
            m_listViewDragItemsCompletedRevoker = listView.DragItemsCompleted(winrt::auto_revoke, { this, &TabView::OnListViewDragItemsCompleted });
            m_listViewDragOverRevoker = listView.DragOver(winrt::auto_revoke, { this, &TabView::OnListViewDragOver });
            m_listViewDropRevoker = listView.Drop(winrt::auto_revoke, { this, &TabView::OnListViewDrop });

            m_listViewGettingFocusRevoker = listView.GettingFocus(winrt::auto_revoke, { this, &TabView::OnListViewGettingFocus });
        }
        return listView;
    }());*/

    m_itemsRepeater.set([this, controlProtected]() {
        auto repeater = GetTemplateChildT<winrt::ItemsRepeater>(L"TabItemsRepeater", controlProtected);
        if (repeater)
        {
            m_repeaterLoadedRevoker = repeater.Loaded(winrt::auto_revoke, { this, &TabView::OnRepeaterLoaded });
            m_repeaterElementPreparedRevoker = repeater.ElementPrepared(winrt::auto_revoke, { this, &TabView::OnRepeaterElementPrepared });
            m_repeaterElementIndexChangedRevoker = repeater.ElementIndexChanged(winrt::auto_revoke, { this, &TabView::OnRepeaterElementIndexChanged });
            //### m_listViewGettingFocusRevoker = repeater.GettingFocus(winrt::auto_revoke, { this, &TabView::OnListViewGettingFocus });
        }
        return repeater;
    }());

    m_selectionChangedRevoker = m_selectionModel.SelectionChanged(winrt::auto_revoke, { this, &TabView::OnSelectionChanged });

    m_addButton.set([this, controlProtected]() {
        auto addButton = GetTemplateChildT<winrt::Button>(L"AddButton", controlProtected);
        if (addButton)
        {
            // Do localization for the add button
            if (winrt::AutomationProperties::GetName(addButton).empty())
            {
                auto addButtonName = ResourceAccessor::GetLocalizedStringResource(SR_TabViewAddButtonName);
                winrt::AutomationProperties::SetName(addButton, addButtonName);
            }

            auto toolTip = winrt::ToolTipService::GetToolTip(addButton);
            if (!toolTip)
            {
                winrt::ToolTip tooltip = winrt::ToolTip();
                tooltip.Content(box_value(ResourceAccessor::GetLocalizedStringResource(SR_TabViewAddButtonTooltip)));
                winrt::ToolTipService::SetToolTip(addButton, tooltip);
            }

            m_addButtonClickRevoker = addButton.Click(winrt::auto_revoke, { this, &TabView::OnAddButtonClick });
        }
        return addButton;
    }());

    if (SharedHelpers::IsThemeShadowAvailable())
    {
        if (auto shadowCaster = GetTemplateChildT<winrt::Grid>(L"ShadowCaster", controlProtected))
        {
            winrt::ThemeShadow shadow;
            shadow.Receivers().Append(GetShadowReceiver());

            double shadowDepth = unbox_value<double>(SharedHelpers::FindResource(c_tabViewShadowDepthName, winrt::Application::Current().Resources(), box_value(c_tabShadowDepth)));

            auto currentTranslation = shadowCaster.Translation();
            auto translation = winrt::float3{ currentTranslation.x, currentTranslation.y, (float)shadowDepth };
            shadowCaster.Translation(translation);

            shadowCaster.Shadow(shadow);
        }
    }
}

void TabView::OnListViewGettingFocus(const winrt::IInspectable& sender, const winrt::GettingFocusEventArgs& args)
{
    // TabViewItems overlap each other by one pixel in order to get the desired visuals for the separator.
    // This causes problems with 2d focus navigation. Because the items overlap, pressing Down or Up from a
    // TabViewItem navigates to the overlapping item which is not desired.
    //
    // To resolve this issue, we detect the case where Up or Down focus navigation moves from one TabViewItem
    // to another.
    // How we handle it, depends on the input device.
    // For GamePad, we want to move focus to something in the direction of movement (other than the overlapping item)
    // For Keyboard, we cancel the focus movement.

    auto direction = args.Direction();
    if (direction == winrt::FocusNavigationDirection::Up || direction == winrt::FocusNavigationDirection::Down)
    {
        auto oldItem = args.OldFocusedElement().try_as<winrt::TabViewItem>();
        auto newItem = args.NewFocusedElement().try_as<winrt::TabViewItem>();
        if (oldItem && newItem)
        {
            if (auto listView = m_listView.get())
            {
                bool oldItemIsFromThisTabView = listView.IndexFromContainer(oldItem) != -1;
                bool newItemIsFromThisTabView = listView.IndexFromContainer(newItem) != -1;
                if (oldItemIsFromThisTabView && newItemIsFromThisTabView)
                {
                    auto inputDevice = args.InputDevice();
                    if (inputDevice == winrt::FocusInputDeviceKind::GameController)
                    {
                        auto listViewBoundsLocal = winrt::Rect{ 0, 0, static_cast<float>(listView.ActualWidth()), static_cast<float>(listView.ActualHeight()) };
                        auto listViewBounds = listView.TransformToVisual(nullptr).TransformBounds(listViewBoundsLocal);
                        winrt::FindNextElementOptions options;
                        options.ExclusionRect(listViewBounds);
                        auto next = winrt::FocusManager::FindNextElement(direction, options);
                        if(auto args2 = args.try_as<winrt::IGettingFocusEventArgs2>())
                        {
                            args2.TrySetNewFocusedElement(next);
                        }
                        else
                        {
                            // Without TrySetNewFocusedElement, we cannot set focus while it is changing.
                            m_dispatcherHelper.RunAsync([next]()
                            {
                                SetFocus(next, winrt::FocusState::Programmatic);
                            });
                        }
                        args.Handled(true);
                    }
                    else
                    {
                        args.Cancel(true);
                        args.Handled(true);
                    }
                }
            }
        }
    }
}

void TabView::OnSelectedIndexPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args)
{
    UpdateSelectedIndex();
}

void TabView::OnSelectedItemPropertyChanged(const winrt::DependencyPropertyChangedEventArgs& args)
{
    UpdateSelectedItem();
}

void TabView::OnTabWidthModePropertyChanged(const winrt::DependencyPropertyChangedEventArgs&)
{
    UpdateTabWidths();
}

void TabView::OnAddButtonClick(const winrt::IInspectable&, const winrt::RoutedEventArgs& args)
{
    m_addTabButtonClickEventSource(*this, args);
}

winrt::AutomationPeer TabView::OnCreateAutomationPeer()
{
    return winrt::make<TabViewAutomationPeer>(*this);
}

void TabView::OnLoaded(const winrt::IInspectable&, const winrt::RoutedEventArgs&)
{
    UpdateTabContent();
}

void TabView::OnRepeaterLoaded(const winrt::IInspectable&, const winrt::RoutedEventArgs& args)
{
    if (auto repeater = m_itemsRepeater.get())
    {
        if (TabItemsSource())
        {
            repeater.ItemsSource(TabItemsSource());
        }
        else
        {
            repeater.ItemsSource(TabItems());
        }


        m_selectionModel.Source(repeater.ItemsSourceView());
        m_collectionChangedRevoker = repeater.ItemsSourceView().CollectionChanged(winrt::auto_revoke, { this, &TabView::OnItemsChanged });

        if (ReadLocalValue(s_SelectedItemProperty) != winrt::DependencyProperty::UnsetValue())
        {
            UpdateSelectedItem();
        }
        else
        {
            // If SelectedItem wasn't set, default to selecting the first tab
            UpdateSelectedIndex();
        }

        if (auto selectedIndex = m_selectionModel.SelectedIndex())
        {
            SelectedIndex(selectedIndex.GetAt(0));
            SelectedItem(m_selectionModel.SelectedItem());
        }
    }

    /* ### if (auto listView = m_listView.get())
    {
        // Find TabsItemsPresenter and listen for SizeChanged
        m_itemsPresenter.set([this, listView]() {
            auto itemsPresenter = SharedHelpers::FindInVisualTreeByName(listView, L"TabsItemsPresenter").as<winrt::ItemsPresenter>();
            if (itemsPresenter)
            {
                m_itemsPresenterSizeChangedRevoker = itemsPresenter.SizeChanged(winrt::auto_revoke, { this, &TabView::OnItemsPresenterSizeChanged });
            }
            return itemsPresenter;
        }());

        auto scrollViewer = SharedHelpers::FindInVisualTreeByName(listView, L"ScrollViewer").as<winrt::FxScrollViewer>();
        m_scrollViewer.set(scrollViewer);
        if (scrollViewer)
        {
            if (SharedHelpers::IsIsLoadedAvailable() && scrollViewer.IsLoaded())
            {
                // This scenario occurs reliably for Terminal in XAML islands
                OnScrollViewerLoaded(nullptr, nullptr);
            }
            else
            {
                m_scrollViewerLoadedRevoker = scrollViewer.Loaded(winrt::auto_revoke, { this, &TabView::OnScrollViewerLoaded });
            }
        }
    }*/
}

void TabView::OnScrollViewerLoaded(const winrt::IInspectable&, const winrt::RoutedEventArgs& args)
{
    if (auto&& scrollViewer = m_scrollViewer.get())
    {
        auto decreaseButton = SharedHelpers::FindInVisualTreeByName(scrollViewer, L"ScrollDecreaseButton").as<winrt::RepeatButton>();
        m_scrollDecreaseClickRevoker = decreaseButton.Click(winrt::auto_revoke, { this, &TabView::OnScrollDecreaseClick });

        auto increaseButton = SharedHelpers::FindInVisualTreeByName(scrollViewer, L"ScrollIncreaseButton").as<winrt::RepeatButton>();
        m_scrollIncreaseClickRevoker = increaseButton.Click(winrt::auto_revoke, { this, &TabView::OnScrollIncreaseClick });
    }

    UpdateTabWidths();
}

void TabView::OnItemsPresenterSizeChanged(const winrt::IInspectable& sender, const winrt::SizeChangedEventArgs& args)
{
    UpdateTabWidths();
}

void TabView::OnItemsChanged(const winrt::IInspectable& dataSource, const winrt::NotifyCollectionChangedEventArgs& args)
{
    //### todo
    //m_tabItemsChangedEventSource(*this, args);

    int numItems = static_cast<int>(TabItems().Size());
    if (args.Action() == winrt::NotifyCollectionChangedAction::Remove && numItems > 0)
    {
        if (SelectedIndex() == static_cast<int32_t>(args.OldStartingIndex()))
        {
            // Find the closest tab to select instead.
            int startIndex = static_cast<int>(args.OldStartingIndex());
            if (startIndex >= numItems)
            {
                startIndex = numItems - 1;
            }
            int index = startIndex;

            do
            {
                auto nextItem = ContainerFromIndex(index).as<winrt::TabViewItem>();

                if (nextItem && nextItem.IsEnabled() && nextItem.Visibility() == winrt::Visibility::Visible)
                {
                    //SelectedItem(Items().GetAt(index));
                    m_selectionModel.Select(index);
                    break;
                }

                // try the next item
                index++;
                if (index >= numItems)
                {
                    index = 0;
                }
            } while (index != startIndex);
        }
    }

    UpdateTabWidths();
}

void TabView::OnSelectionChanged(const winrt::SelectionModel& sender, const winrt::SelectionModelSelectionChangedEventArgs& args)
{
    /*WCHAR strOut[1024];
    auto index = sender.SelectedIndex().GetAt(0);
    StringCchPrintf(strOut, ARRAYSIZE(strOut), L"SelectedIndex: %d\n", index);
    OutputDebugString(strOut);*/

    auto item = sender.SelectedItem();
    if (item)
    {
        OutputDebugString(L"SelectedItem exists\n");
    }
    else
    {
        if (auto repeater = m_itemsRepeater.get())
        {
            if (sender.SelectedIndex())
            {
                auto index = sender.SelectedIndex().GetAt(0);
                item = repeater.TryGetElement(index);
            }
        }
    }

    SelectedItem(item);
    if (sender.SelectedIndex())
    {
        SelectedIndex(sender.SelectedIndex().GetAt(0));
    }

    UpdateTabContent();

    // ### TODO: make some args
    auto eventArgs = winrt::make_self<TabViewSelectionChangedEventArgs>(SelectedIndex());
    m_selectionChangedEventSource(*this, *eventArgs);
}

winrt::TabViewItem TabView::FindTabViewItemFromDragItem(const winrt::IInspectable& item)
{
    auto tab = ContainerFromItem(item).try_as<winrt::TabViewItem>();

    if (!tab)
    {
        if (auto fe = item.try_as<winrt::FrameworkElement>())
        {
            tab = winrt::VisualTreeHelper::GetParent(fe).try_as<winrt::TabViewItem>();
        }
    }

    if (!tab)
    {
        // This is a fallback scenario for tabs without a data context
        auto numItems = static_cast<int>(TabItems().Size());
        for (int i = 0; i < numItems; i++)
        {
            auto tabItem = ContainerFromIndex(i).try_as<winrt::TabViewItem>();
            if (tabItem.Content() == item)
            {
                tab = tabItem;
                break;
            }
        }
    }

    return tab;
}

void TabView::OnListViewDragItemsStarting(const winrt::IInspectable& sender, const winrt::DragItemsStartingEventArgs& args)
{
    auto item = args.Items().GetAt(0);
    auto tab = FindTabViewItemFromDragItem(item);
    auto myArgs = winrt::make_self<TabViewTabDragStartingEventArgs>(args, item, tab);

    m_tabDragStartingEventSource(*this, *myArgs);
}

void TabView::OnListViewDragOver(const winrt::IInspectable& sender, const winrt::DragEventArgs& args)
{
    m_tabStripDragOverEventSource(*this, args);
}

void TabView::OnListViewDrop(const winrt::IInspectable& sender, const winrt::DragEventArgs& args)
{
    m_tabStripDropEventSource(*this, args);
}

void TabView::OnListViewDragItemsCompleted(const winrt::IInspectable& sender, const winrt::DragItemsCompletedEventArgs& args)
{
    auto item = args.Items().GetAt(0);
    auto tab = FindTabViewItemFromDragItem(item);
    auto myArgs = winrt::make_self<TabViewTabDragCompletedEventArgs>(args, item, tab);

    m_tabDragCompletedEventSource(*this, *myArgs);

    // None means it's outside of the tab strip area
    if (args.DropResult() == winrt::DataPackageOperation::None)
    {
        auto tabDroppedArgs = winrt::make_self<TabViewTabDroppedOutsideEventArgs>(item, tab);
        m_tabDroppedOutsideEventSource(*this, *tabDroppedArgs);
    }
}

void TabView::UpdateTabContent()
{
    if (auto tabContentPresenter = m_tabContentPresenter.get())
    {
        if (!SelectedItem())
        {
            tabContentPresenter.Content(nullptr);
            tabContentPresenter.ContentTemplate(nullptr);
            tabContentPresenter.ContentTemplateSelector(nullptr);
        }
        else
        {
            auto tvi = ContainerFromItem(SelectedItem()).try_as<winrt::TabViewItem>();

            if (tvi)
            {
                // If the focus was in the old tab content, we will lose focus when it is removed from the visual tree.
                // We should move the focus to the new tab content.
                // The new tab content is not available at the time of the LosingFocus event, so we need to
                // move focus later.
                bool shouldMoveFocusToNewTab = false;
                auto revoker = tabContentPresenter.LosingFocus(winrt::auto_revoke, [&shouldMoveFocusToNewTab](const winrt::IInspectable&, const winrt::LosingFocusEventArgs& args)
                {
                    shouldMoveFocusToNewTab = true;
                });

                tabContentPresenter.Content(tvi.Content());
                tabContentPresenter.ContentTemplate(tvi.ContentTemplate());
                tabContentPresenter.ContentTemplateSelector(tvi.ContentTemplateSelector());

                // It is not ideal to call UpdateLayout here, but it is necessary to ensure that the ContentPresenter has expanded its content
                // into the live visual tree.
                tabContentPresenter.UpdateLayout();

                if (shouldMoveFocusToNewTab)
                {
                    auto focusable = winrt::FocusManager::FindFirstFocusableElement(tabContentPresenter);
                    if (!focusable)
                    {
                        // If there is nothing focusable in the new tab, just move focus to the TabViewItem itself.
                        focusable = tvi;
                    }

                    if (focusable)
                    {
                        SetFocus(focusable, winrt::FocusState::Programmatic);
                    }
                }
            }
        }
    }
}

void TabView::RequestCloseTab(winrt::TabViewItem const& container)
{
    if (auto listView = m_listView.get())
    {
        auto args = winrt::make_self<TabViewTabCloseRequestedEventArgs>(listView.ItemFromContainer(container), container);
            
        m_tabCloseRequestedEventSource(*this, *args);

        if (auto internalTabViewItem = winrt::get_self<TabViewItem>(container))
        {
            internalTabViewItem->RaiseRequestClose(*args);
        }
    }
}

void TabView::OnScrollDecreaseClick(const winrt::IInspectable&, const winrt::RoutedEventArgs&)
{
    if (auto scrollViewer = m_scrollViewer.get())
    {
        scrollViewer.ChangeView(std::max(0.0, scrollViewer.HorizontalOffset() - c_scrollAmount), nullptr, nullptr);
    }
}

void TabView::OnScrollIncreaseClick(const winrt::IInspectable&, const winrt::RoutedEventArgs&)
{
    if (auto scrollViewer = m_scrollViewer.get())
    {
        scrollViewer.ChangeView(std::min(scrollViewer.ScrollableWidth(), scrollViewer.HorizontalOffset() + c_scrollAmount), nullptr, nullptr);
    }
}

winrt::Size TabView::MeasureOverride(winrt::Size const& availableSize)
{
    if (previousAvailableSize.Width != availableSize.Width)
    {
        previousAvailableSize = availableSize;
        UpdateTabWidths();
    }

    return __super::MeasureOverride(availableSize);
}

void TabView::UpdateTabWidths()
{
    double tabWidth = std::numeric_limits<double>::quiet_NaN();

    if (auto tabGrid = m_tabContainerGrid.get())
    {
        // Add up width taken by custom content and + button
        double widthTaken = 0.0;
        if (auto leftContentColumn = m_leftContentColumn.get())
        {
            widthTaken += leftContentColumn.ActualWidth();
        }
        if (auto addButtonColumn = m_addButtonColumn.get())
        {
            widthTaken += addButtonColumn.ActualWidth();
        }
        if (auto&& rightContentColumn = m_rightContentColumn.get())
        {
            if (auto rightContentPresenter = m_rightContentPresenter.get())
            {
                winrt::Size rightContentSize = rightContentPresenter.DesiredSize();
                rightContentColumn.MinWidth(rightContentSize.Width);
                widthTaken += rightContentSize.Width;
            }
        }

        if (auto tabColumn = m_tabColumn.get())
        {
            // Note: can be infinite
            auto availableWidth = previousAvailableSize.Width - widthTaken;

            // Size can be 0 when window is first created; in that case, skip calculations; we'll get a new size soon
            if (availableWidth > 0)
            {
                if (TabWidthMode() == winrt::TabViewWidthMode::SizeToContent)
                {
                    tabColumn.MaxWidth(availableWidth);
                    tabColumn.Width(winrt::GridLengthHelper::FromValueAndType(1.0, winrt::GridUnitType::Auto));
                    if (auto listview = m_listView.get())
                    {
                        listview.MaxWidth(availableWidth);

                        // Calculate if the scroll buttons should be visible.
                        if (auto itemsPresenter = m_itemsPresenter.get())
                        {
                            winrt::FxScrollViewer::SetHorizontalScrollBarVisibility(listview, itemsPresenter.ActualWidth() > availableWidth
                                ? winrt::Windows::UI::Xaml::Controls::ScrollBarVisibility::Visible
                                : winrt::Windows::UI::Xaml::Controls::ScrollBarVisibility::Hidden);
                        }
                    }
                }
                else if (TabWidthMode() == winrt::TabViewWidthMode::Equal)
                {
                    auto const minTabWidth = unbox_value<double>(SharedHelpers::FindResource(c_tabViewItemMinWidthName, winrt::Application::Current().Resources(), box_value(c_tabMinimumWidth)));
                    auto const maxTabWidth = unbox_value<double>(SharedHelpers::FindResource(c_tabViewItemMaxWidthName, winrt::Application::Current().Resources(), box_value(c_tabMaximumWidth)));

                    // Calculate the proportional width of each tab given the width of the ScrollViewer.
                    auto const padding = Padding();
                    auto const tabWidthForScroller = (availableWidth - (padding.Left + padding.Right)) / (double)(TabItems().Size());

                    tabWidth = std::clamp(tabWidthForScroller, minTabWidth, maxTabWidth);

                    // Size tab column to needed size
                    tabColumn.MaxWidth(availableWidth);
                    auto requiredWidth = tabWidth * TabItems().Size();
                    if (requiredWidth >= availableWidth)
                    {
                        tabColumn.Width(winrt::GridLengthHelper::FromPixels(availableWidth));
                        if (auto listview = m_listView.get())
                        {
                            winrt::FxScrollViewer::SetHorizontalScrollBarVisibility(listview, winrt::Windows::UI::Xaml::Controls::ScrollBarVisibility::Visible);
                        }
                    }
                    else
                    {
                        tabColumn.Width(winrt::GridLengthHelper::FromValueAndType(1.0, winrt::GridUnitType::Auto));
                        if (auto listview = m_listView.get())
                        {
                            winrt::FxScrollViewer::SetHorizontalScrollBarVisibility(listview, winrt::Windows::UI::Xaml::Controls::ScrollBarVisibility::Hidden);
                        }
                    }
                }
            }
        }
    }

    for (auto item : TabItems())
    {
        // Set the calculated width on each tab.
        auto tvi = item.try_as<winrt::TabViewItem>();
        if (!tvi)
        {
            tvi = ContainerFromItem(item).as<winrt::TabViewItem>();
        }

        if (tvi)
        {
            tvi.Width(tabWidth);
        }
    }
}

void TabView::UpdateSelectedItem()
{
    uint32_t index;
    if (TabItems().IndexOf(SelectedItem(), index))
    {
        m_selectionModel.Select(index);
    }
}

void TabView::UpdateSelectedIndex()
{
    if (!m_selectionModel.SelectedIndex() ||
        m_selectionModel.SelectedIndex().GetAt(0) != SelectedIndex())
    {
        m_selectionModel.Select(SelectedIndex());
    }
}

winrt::DependencyObject TabView::ContainerFromItem(winrt::IInspectable const& item)
{
    auto tvi = SelectedItem().try_as<winrt::TabViewItem>();
    if (!tvi)
    {
        if (auto repeater = m_itemsRepeater.get())
        {
            uint32_t index;
            if (TabItems().IndexOf(item, index))
            {
                tvi = repeater.TryGetElement(index).try_as<winrt::TabViewItem>();
            }
        }
    }
    return tvi;
}

winrt::DependencyObject TabView::ContainerFromIndex(int index)
{
    if (auto repeater = m_itemsRepeater.get())
    {
        return repeater.TryGetElement(index).try_as<winrt::TabViewItem>();
    }
    return nullptr;
}

winrt::IInspectable TabView::ItemFromContainer(winrt::DependencyObject const& container)
{
    // ### todo
    if (auto listView = m_listView.get())
    {
        return listView.ItemFromContainer(container);
    }
    return nullptr;
}

int TabView::GetItemCount()
{
    if (auto itemssource = TabItemsSource())
    {
        if (auto iterable = itemssource.try_as<winrt::IIterable<winrt::IInspectable>>())
        {
            int i = 1;
            auto iter = iterable.First();
            while (iter.MoveNext())
            {
                i++;
            }
            return i;
        }
        return 0;
    }
    else
    {
        return static_cast<int>(TabItems().Size());
    }
}

bool TabView::SelectNextTab(int increment)
{
    bool handled = false;
    const int itemsSize = GetItemCount();
    if (itemsSize > 1)
    {
        auto index = SelectedIndex();
        index = (index + increment + itemsSize) % itemsSize;
        SelectedIndex(index);
        handled = true;
    }
    return handled;
}

bool TabView::RequestCloseCurrentTab()
{
    bool handled = false;
    if (auto selectedTab = SelectedItem().try_as<winrt::TabViewItem>())
    {
        if (selectedTab.IsClosable())
        {
            // Close the tab on ctrl + F4
            RequestCloseTab(selectedTab);
            handled = true;
        }
    }

    return handled;
}

void TabView::OnKeyDown(winrt::KeyRoutedEventArgs const& args)
{
    if (auto coreWindow = winrt::CoreWindow::GetForCurrentThread())
    {
        if (args.Key() == winrt::VirtualKey::F4)
        {
            // Handle Ctrl+F4 on RS2 and lower
            // On RS3+, it is handled by a KeyboardAccelerator
            if (!SharedHelpers::IsRS3OrHigher())
            {
                auto isCtrlDown = (coreWindow.GetKeyState(winrt::VirtualKey::Control) & winrt::CoreVirtualKeyStates::Down) == winrt::CoreVirtualKeyStates::Down;
                if (isCtrlDown)
                {
                    args.Handled(RequestCloseCurrentTab());
                }
            }
        }
        else if (args.Key() == winrt::VirtualKey::Tab)
        {
            // Handle Ctrl+Tab/Ctrl+Shift+Tab on RS5 and lower
            // On 19H1+, it is handled by a KeyboardAccelerator
            if (!SharedHelpers::Is19H1OrHigher())
            {
                auto isCtrlDown = (coreWindow.GetKeyState(winrt::VirtualKey::Control) & winrt::CoreVirtualKeyStates::Down) == winrt::CoreVirtualKeyStates::Down;
                auto isShiftDown = (coreWindow.GetKeyState(winrt::VirtualKey::Shift) & winrt::CoreVirtualKeyStates::Down) == winrt::CoreVirtualKeyStates::Down;

                if (isCtrlDown && !isShiftDown)
                {
                    args.Handled(SelectNextTab(1));
                }
                else if (isCtrlDown && isShiftDown)
                {
                    args.Handled(SelectNextTab(-1));
                }
            }
        }
    }
}

void TabView::OnCtrlF4Invoked(const winrt::KeyboardAccelerator& sender, const winrt::KeyboardAcceleratorInvokedEventArgs& args)
{
    args.Handled(RequestCloseCurrentTab());
}

void TabView::OnCtrlTabInvoked(const winrt::KeyboardAccelerator& sender, const winrt::KeyboardAcceleratorInvokedEventArgs& args)
{
    args.Handled(SelectNextTab(1));
}

void TabView::OnCtrlShiftTabInvoked(const winrt::KeyboardAccelerator& sender, const winrt::KeyboardAcceleratorInvokedEventArgs& args)
{
    args.Handled(SelectNextTab(-1));
}

void TabView::OnRepeaterElementPrepared(const winrt::ItemsRepeater& sender, const winrt::ItemsRepeaterElementPreparedEventArgs& args)
{
    if (auto item = args.Element().try_as<winrt::TabViewItem>())
    {
        auto tabItem = winrt::get_self<TabViewItem>(item);
        tabItem->RepeatedIndex(args.Index());
        //### tabItem->SelectionModel(m_selectionModel);
    }
}

void TabView::OnRepeaterElementIndexChanged(const winrt::ItemsRepeater& sender, const winrt::ItemsRepeaterElementIndexChangedEventArgs& args)
{
    if (auto item = args.Element().try_as<winrt::TabViewItem>())
    {
        auto tabItem = winrt::get_self<TabViewItem>(item);
        tabItem->RepeatedIndex(args.NewIndex());
    }
}
