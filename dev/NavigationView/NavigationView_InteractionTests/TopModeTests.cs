﻿// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

using Common;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Windows.UI.Xaml.Tests.MUXControls.InteractionTests.Infra;
using Windows.UI.Xaml.Tests.MUXControls.InteractionTests.Common;

#if USING_TAEF
using WEX.TestExecution;
using WEX.TestExecution.Markup;
using WEX.Logging.Interop;
#else
using Microsoft.VisualStudio.TestTools.UnitTesting;
using Microsoft.VisualStudio.TestTools.UnitTesting.Logging;
#endif

using Microsoft.Windows.Apps.Test.Automation;
using Microsoft.Windows.Apps.Test.Foundation;
using Microsoft.Windows.Apps.Test.Foundation.Controls;
using Microsoft.Windows.Apps.Test.Foundation.Waiters;

namespace Windows.UI.Xaml.Tests.MUXControls.InteractionTests.NavigationViewTests
{
    [TestClass]
    public class TopModeTests : NavigationViewTestsBase
    {

        [ClassInitialize]
        [TestProperty("RunAs", "User")]
        [TestProperty("Classification", "Integration")]
        [TestProperty("TestPass:IncludeOnlyOn", "Desktop")]
        [TestProperty("MUXControlsTestEnabledForPhone", "True")]
        public static void ClassInitialize(TestContext testContext)
        {
            TestEnvironment.Initialize(testContext);
        }

        [TestMethod] //bug 18033309
        public void TopNavigationSecondClickOnSuppressSelectionItemTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                Button resetResultButton = new Button(FindElement.ById("ResetResult"));
                UIObject suppressSelection = FindElement.ByName("SuppressSelection");

                var invokeResult = new Edit(FindElement.ById("ItemInvokedResult"));
                var selectResult = new Edit(FindElement.ById("SelectionChangedResult"));
                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    suppressSelection.Click();
                    waiter.Wait();
                }

                // First time selection only raise ItemInvoke
                Verify.AreEqual(invokeResult.Value, "SuppressSelection");
                Verify.AreEqual(selectResult.Value, "");

                resetResultButton.Click();
                Wait.ForIdle();

                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    suppressSelection.Click();
                    waiter.Wait();
                }

                // Click it again, only raise ItemInvoke event
                Verify.AreEqual(invokeResult.Value, "SuppressSelection");
                Verify.AreEqual(selectResult.Value, "");
            }
        }

        [TestMethod]
        public void TopNavigationWithAccessKeysTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                Log.Comment("Add ten items to make overflow happen");
                Button addTenItems = new Button(FindElement.ByName("AddTenItems"));
                addTenItems.Invoke();
                Wait.ForIdle();

                Log.Comment("Verify overflow menu is not opened");
                Verify.IsTrue(GetTopNavigationItems(TopNavPosition.Overflow).Count == 0);

                InvokeNavigationViewAccessKeyAndVerifyKeyTipPlacement("TopNavOverflowButton");

                Log.Comment("Verify overflow menu is opened");
                // Flyout doesn't seem raise any UIA WindowOpened/MenuOpened events so just check a few times for the menu to
                // have opened.
                TestEnvironment.VerifyAreEqualWithRetry(5,
                    () => true,
                    () => GetTopNavigationItems(TopNavPosition.Overflow).Count > 0);
            }
        }

        [TestMethod]
        public void TopNavigationOverflowWidthLongNavItemTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                var longNavItemPartialContent = "Gates";
                var primaryCount = GetTopNavigationItems(TopNavPosition.Primary).Count;

                Log.Comment("Add a long navigationview item which text includes " + longNavItemPartialContent);
                Button button = new Button(FindElement.ByName("AddLongNavItem"));
                button.Invoke();
                Wait.ForIdle();

                var count = GetTopNavigationItems(TopNavPosition.Primary).Count;
                Verify.AreEqual(primaryCount, count, "The appended nav item goes to overflow");

                // Select the longest nav item
                OpenOverflowMenuAndInvokeItem(longNavItemPartialContent);

                count = GetTopNavigationItems(TopNavPosition.Primary).Count;
                Verify.IsTrue(primaryCount - count >= 2, "Longest nav item make more than 1 items to overflow " + primaryCount + " vs " + count);

                // Select the shortest IntegerItem which content is 7
                OpenOverflowMenuAndInvokeItem("IntegerItem");
                Verify.IsTrue(
                    GetTopNavigationItems(TopNavPosition.Primary).
                        Where(item => UIObjectContains(item, longNavItemPartialContent)).
                        Count() == 0,
                    "Longest nav item is pushed to overflow");

                count = GetTopNavigationItems(TopNavPosition.Primary).Count;
                Verify.IsTrue(primaryCount <= count,
                    "Select the shortest item make more item to primary " + primaryCount + " vs " + count);
            }
        }

        [TestMethod]
        public void TopNavigationOverflowButtonTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                Log.Comment("Add ten items to make overflow happen");
                Button addTenItems = new Button(FindElement.ByName("AddTenItems"));
                addTenItems.Invoke();
                Wait.ForIdle();

                Log.Comment("Get and Check that active visualstate contains OverflowButtonWithLabel");
                var getActiveVisualStateButton = new Button(FindElement.ByName("GetActiveVisualState"));
                var invokeResult = new Edit(FindElement.ById("TestResult"));
                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    getActiveVisualStateButton.Click();
                    waiter.Wait();
                }

                Verify.IsTrue(invokeResult.Value.Contains("OverflowButtonWithLabel"));

                Log.Comment("Hide the overflow button label");
                var changeOverflowLabelVisibility = new CheckBox(FindElement.ByName("ChangeOverflowLabelVisibility"));
                changeOverflowLabelVisibility.Uncheck();
                Wait.ForIdle();

                Log.Comment("Get and Check that active visualstate contains OverflowButtonNoLabel");
                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    getActiveVisualStateButton.Click();
                    waiter.Wait();
                }

                Verify.IsTrue(invokeResult.Value.Contains("OverflowButtonNoLabel"));
            }
        }

        [TestMethod]
        public void TopNavigationSelectionTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                if (!PlatformConfiguration.IsOsVersionGreaterThanOrEqual(OSVersion.Redstone2))
                {
                    Log.Warning("Skipping: only works in RS2 and above");
                    return;
                }


                Button resetResultButton = new Button(FindElement.ById("ResetResult"));
                UIObject home = FindElement.ByName("Home");
                UIObject apps = FindElement.ById("AppsItem");
                UIObject suppressSelection = FindElement.ByName("SuppressSelection");

                var invokeResult = new Edit(FindElement.ById("ItemInvokedResult"));
                var selectResult = new Edit(FindElement.ById("SelectionChangedResult"));
                var invokeRecommendedTransition = new Edit(FindElement.ById("InvokeRecommendedTransition"));
                var selectionChangeRecommendedTransition = new Edit(FindElement.ById("SelectionChangeRecommendedTransition"));
                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    apps.Click();
                    waiter.Wait();
                }

                // First time selection raise ItemInvoke and SelectionChange events
                Verify.AreEqual(invokeResult.Value, "Apps");
                Verify.AreEqual(selectResult.Value, "Apps");
                Verify.AreEqual(invokeRecommendedTransition.Value, "Default");
                Verify.AreEqual(selectionChangeRecommendedTransition.Value, "Default");

                resetResultButton.Click();
                Wait.ForIdle();

                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    apps.Click();
                    waiter.Wait();
                }

                // Click it again, only raise ItemInvoke event
                Verify.AreEqual(invokeResult.Value, "Apps");
                Verify.AreEqual(selectResult.Value, "");
                Verify.AreEqual(invokeRecommendedTransition.Value, "Default");
                Verify.AreEqual(selectionChangeRecommendedTransition.Value, "");

                resetResultButton.Click();
                Wait.ForIdle();

                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    suppressSelection.Click();
                    waiter.Wait();
                }

                // Only click for suppress items
                Verify.AreEqual(invokeResult.Value, "SuppressSelection");
                Verify.AreEqual(selectResult.Value, "");
                Verify.AreEqual(invokeRecommendedTransition.Value, "Default");
                Verify.AreEqual(selectionChangeRecommendedTransition.Value, "");

                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    home.Click();
                    waiter.Wait();
                }

                // Click home again, it raise two events. transition from right to left
                Verify.AreEqual(invokeResult.Value, "Home");
                Verify.AreEqual(selectResult.Value, "Home");

                // Only RS5 or above supports SlideNavigationTransitionInfo
                if (PlatformConfiguration.IsOsVersionGreaterThanOrEqual(OSVersion.Redstone5))
                {
                    Verify.AreEqual(invokeRecommendedTransition.Value, "FromLeft");
                    Verify.AreEqual(selectionChangeRecommendedTransition.Value, "FromLeft");
                }

                resetResultButton.Click();
                Wait.ForIdle();

                // click apps again. transition from left to right
                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    apps.Click();
                    waiter.Wait();
                }

                Verify.AreEqual(invokeResult.Value, "Apps");
                Verify.AreEqual(selectResult.Value, "Apps");

                // Only RS5 or above supports SlideNavigationTransitionInfo
                if (PlatformConfiguration.IsOsVersionGreaterThanOrEqual(OSVersion.Redstone5))
                {
                    Verify.AreEqual(invokeRecommendedTransition.Value, "FromRight");
                    Verify.AreEqual(selectionChangeRecommendedTransition.Value, "FromRight");
                }
            }
        }

        [TestMethod]
        public void TopNavigationSetSelectedItemToNullInItemInvoke()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                if (!PlatformConfiguration.IsOsVersionGreaterThanOrEqual(OSVersion.Redstone2))
                {
                    Log.Warning("Skipping: only works in RS2 and above");
                    return;
                }


                Button resetResultButton = new Button(FindElement.ById("ResetResult"));
                UIObject home = FindElement.ByName("Home");
                UIObject apps = FindElement.ById("AppsItem");

                var invokeResult = new Edit(FindElement.ById("ItemInvokedResult"));
                var selectResult = new Edit(FindElement.ById("SelectionChangedResult"));
                var invokeRecommendedTransition = new Edit(FindElement.ById("InvokeRecommendedTransition"));
                var selectionChangeRecommendedTransition = new Edit(FindElement.ById("SelectionChangeRecommendedTransition"));
                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    apps.Click();
                    waiter.Wait();
                }

                // First time selection raise ItemInvoke and SelectionChange events
                Verify.AreEqual(invokeResult.Value, "Apps");
                Verify.AreEqual(selectResult.Value, "Apps");
                Verify.AreEqual(invokeRecommendedTransition.Value, "Default");
                Verify.AreEqual(selectionChangeRecommendedTransition.Value, "Default");

                resetResultButton.Click();
                Wait.ForIdle();

                Button expectNullSelectedItemInItemInvoke = new Button(FindElement.ById("ExpectNullSelectedItemInItemInvoke"));
                expectNullSelectedItemInItemInvoke.Click();
                Wait.ForIdle();

                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    home.Click();
                    waiter.Wait();
                }

                // Click home, expect Null in select change event 
                Verify.AreEqual(invokeResult.Value, "Home");
                Verify.AreEqual(selectResult.Value, "Null");

                // Only RS5 or above supports SlideNavigationTransitionInfo
                if (PlatformConfiguration.IsOsVersionGreaterThanOrEqual(OSVersion.Redstone5))
                {
                    Verify.AreEqual(invokeRecommendedTransition.Value, "FromLeft");
                    Verify.AreEqual(selectionChangeRecommendedTransition.Value, "Default");
                }
            }
        }

        [TestMethod]
        public void TopNavigationOverflowButtonClickTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                Log.Comment("Setting focus to Home");
                UIObject home = FindElement.ByName("Home");
                home.SetFocus();
                Wait.ForIdle();

                Log.Comment("Add ten items to make overflow happen");
                Button addTenItems = new Button(FindElement.ByName("AddTenItems"));
                addTenItems.Invoke();
                Wait.ForIdle();

                InvokeOverflowButton();

                UIObject overflowItem = FindElement.ByName("Added Item 5");
                var invokeResult = new Edit(FindElement.ById("ItemInvokedResult"));
                var selectResult = new Edit(FindElement.ById("SelectionChangedResult"));
                var invokeRecommendedTransition = new Edit(FindElement.ById("InvokeRecommendedTransition"));
                var selectionChangeRecommendedTransition = new Edit(FindElement.ById("SelectionChangeRecommendedTransition"));
                using (var waiter = new ValueChangedEventWaiter(invokeResult))
                {
                    overflowItem.Click();
                    waiter.Wait();
                }

                // First time selection raise ItemInvoke and SelectionChange events
                Verify.AreEqual(invokeResult.Value, "Added Item 5");
                Verify.AreEqual(selectResult.Value, "Added Item 5");

                // only RS5 or above supports SlideNavigationTransitionInfo
                if (PlatformConfiguration.IsOsVersionGreaterThanOrEqual(OSVersion.Redstone5))
                {
                    Verify.AreEqual(invokeRecommendedTransition.Value, "FromRight");
                    Verify.AreEqual(selectionChangeRecommendedTransition.Value, "FromRight");
                }
            }
        }

        [TestMethod]
        public void TopNavigationItemsAccessibilitySetTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                Log.Comment("Setting focus to Home");
                UIObject home = FindElement.ByName("Home");
                home.SetFocus();
                Wait.ForIdle();

                AutomationElement ae = AutomationElement.FocusedElement;
                int positionInSet = (int)ae.GetCurrentPropertyValue(AutomationElement.PositionInSetProperty);
                int sizeOfSet = (int)ae.GetCurrentPropertyValue(AutomationElement.SizeOfSetProperty);

                Verify.AreEqual(1, positionInSet, "Position in set");
                Verify.AreEqual(4, sizeOfSet, "Size of set");


                Log.Comment("Add ten items to make overflow happen");
                Button addTenItems = new Button(FindElement.ByName("AddTenItems"));
                addTenItems.Invoke();
                Wait.ForIdle();

                InvokeOverflowButton();

                UIObject overflowItem = FindElement.ByName("Added Item 5");
                overflowItem.SetFocus();
                Wait.ForIdle();

                ae = AutomationElement.FocusedElement;
                positionInSet = (int)ae.GetCurrentPropertyValue(AutomationElement.PositionInSetProperty);
                sizeOfSet = (int)ae.GetCurrentPropertyValue(AutomationElement.SizeOfSetProperty);

                Verify.AreEqual(8, positionInSet, "Position in overflow.");
                Verify.AreEqual(13, sizeOfSet, "Size of set.");
            }
        }

        [TestMethod]
        public void TopNavigationMenuItemTemplateBindingTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "NavigationView ItemTemplate Test" }))
            {
                ElementCache.Refresh();

                // If binding has problem, we will not see the button and text.
                var lastName = new Button(FindElement.ByName("Anderberg"));
                Verify.IsNotNull(lastName);

                var firstName = new Button(FindElement.ByName("Michael"));
                Verify.IsNotNull(firstName);
            }
        }

        // Bug 17512989. If we change the menu items for multiple times, the item may be not selected.
        [TestMethod]
        public void TopNavigationHaveCorrectSelectionWhenChangingMenuItems()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                for (int i = 0; i < 3; i++)
                {
                    Log.Comment("Iteration: " + i);
                    Log.Comment("Invoke ChangeDataSource");
                    var button = new Button(FindElement.ById("ChangeDataSource"));
                    button.Invoke();
                    Wait.ForIdle();

                    Log.Comment("Reset TestResult");
                    var resetButton = new Button(FindElement.ById("ResetResult"));
                    resetButton.Invoke();
                    Wait.ForIdle();

                    ElementCache.Refresh();
                    UIObject selectedItem = FindElement.ByName("Happy new year Item");

                    Log.Comment("Verify the item is selected");
                    Verify.IsTrue(Convert.ToBoolean(selectedItem.GetProperty(UIProperty.Get("SelectionItem.IsSelected"))));
                }
            }
        }

        [TestMethod]
        public void TopPaddingTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Store Test" }))
            {
                var moveContentUnderTitleBarButton = new Button(FindElement.ById("MoveContentUnderTopnavTitleBar"));
                var flipIsTitleBarAutoPaddingEnabledButton = new Button(FindElement.ById("FlipIsTitleBarAutoPaddingEnabledButton"));
                var getTopPaddingHeightButton = new Button(FindElement.ById("GetTopPaddingHeightButton"));
                var fullScreenButton = new Button(FindElement.ById("FullScreenInvokerButton"));
                var navViewIsTitleBarAutoPaddingEnabledId = "NavViewIsTitleBarAutoPaddingEnabled";
                var topPaddingRenderedValueId = "TopPaddingRenderedValue";
                UIObject navViewIsTitleBarAutoPaddingEnabled = null;
                UIObject topNavTopPadding = null;

                // Checking top padding is added for regular Desktop
                Log.Comment("Setting TitleBar.ExtendViewIntoTitleBar to True");
                moveContentUnderTitleBarButton.Click();
                Wait.ForIdle();

                Log.Comment("Accessing TopPadding Height");
                getTopPaddingHeightButton.Click();
                Wait.ForIdle();

                navViewIsTitleBarAutoPaddingEnabled = TryFindElement.ById(navViewIsTitleBarAutoPaddingEnabledId);
                Verify.IsNotNull(navViewIsTitleBarAutoPaddingEnabled);
                Log.Comment($"NavView.IsTitleBarAutoPaddingEnabled: {navViewIsTitleBarAutoPaddingEnabled.GetText()}");
                Verify.AreEqual("True", navViewIsTitleBarAutoPaddingEnabled.GetText());

                topNavTopPadding = TryFindElement.ById(topPaddingRenderedValueId);
                Verify.IsNotNull(topNavTopPadding);
                Log.Comment($"TopPadding Height: {topNavTopPadding.GetText()}");

                if (!PlatformConfiguration.IsOsVersionGreaterThanOrEqual(OSVersion.Redstone5))
                {
                    Verify.AreEqual("32", topNavTopPadding.GetText());
                }
                else
                {
                    // To detect if it's in tabbed sets, CoreApplicationViewTitleBar.IsVisible can be used to check in MUXControlsTestApp
                    // Not all branches enabled tabbed shell, also not all default setting with tabbed shell enabled
                    // We skip the test if tabbedshell is disabled.
                    if (TryFindElement.ById("TitleBarIsVisible").GetText().Equals("True"))
                    {
                        Log.Comment("Tabbed Shell is disabled or not enabled for this application, skip Verify");
                    }
                    else
                    {
                        Verify.AreEqual("0", topNavTopPadding.GetText());
                    }
                }

                Log.Comment("Setting IsTitleBarAutoPaddingEnabled to False");
                flipIsTitleBarAutoPaddingEnabledButton.Click();
                Wait.ForIdle();

                Log.Comment($"NavView.IsTitleBarAutoPaddingEnabled: {navViewIsTitleBarAutoPaddingEnabled.GetText()}");
                Verify.AreEqual("False", navViewIsTitleBarAutoPaddingEnabled.GetText());

                Log.Comment("Accessing TopPadding Height");
                getTopPaddingHeightButton.Click();
                Wait.ForIdle();
                Log.Comment($"TopPadding Height: {topNavTopPadding.GetText()}");
                Verify.AreEqual("0", topNavTopPadding.GetText());

                Log.Comment("Setting IsTitleBarAutoPaddingEnabled to True");
                flipIsTitleBarAutoPaddingEnabledButton.Click();
                Wait.ForIdle();

                Log.Comment($"NavView.IsTitleBarAutoPaddingEnabled: {navViewIsTitleBarAutoPaddingEnabled.GetText()}");
                Verify.AreEqual("True", navViewIsTitleBarAutoPaddingEnabled.GetText());

                // Checking top padding is NOT added for fullscreen Desktop
                Log.Comment("Setting TitleBar.ExtendViewIntoTitleBar to False");
                moveContentUnderTitleBarButton.Click();
                Wait.ForIdle();
                fullScreenButton.Click();
                Wait.ForIdle();

                Log.Comment("Setting TitleBar.ExtendViewIntoTitleBar to True");
                moveContentUnderTitleBarButton.Click();
                Wait.ForIdle();

                Log.Comment("Accessing TopPadding Height");
                getTopPaddingHeightButton.Click();
                Wait.ForIdle();
                Log.Comment($"TopPadding Height: {topNavTopPadding.GetText()}");
                Verify.AreEqual("0", topNavTopPadding.GetText());

                // Reverting changes to leave app in original state
                Log.Comment("Setting TitleBar.ExtendViewIntoTitleBar to False");
                moveContentUnderTitleBarButton.Click();
                Wait.ForIdle();
                fullScreenButton.Click();
                Wait.ForIdle();
            }
        }

        [TestMethod]
        public void VerifyTopNavigationMinimalVisualStateOnTopNav()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "NavigationView Test" }))
            {
                Log.Comment("To Minimal mode");
                SetNavViewWidth(ControlWidth.Narrow);

                Log.Comment("Get NavView Active VisualStates");
                var getNavViewActiveVisualStatesButton = new Button(FindElement.ByName("GetNavViewActiveVisualStates"));
                getNavViewActiveVisualStatesButton.Invoke();
                Wait.ForIdle();

                var visualStateName = "TopNavigationMinimal";
                var result = new TextBlock(FindElement.ByName("NavViewActiveVisualStatesResult"));
                Verify.IsFalse(result.GetText().Contains(visualStateName), "active VisualStates doesn't include " + visualStateName);

                Log.Comment("To Wide mode");
                SetNavViewWidth(ControlWidth.Wide);

                Log.Comment("Flipping orientation: Left -> Top.");
                var flipOrientationButton = new Button(FindElement.ByName("FlipOrientationButton"));
                flipOrientationButton.Invoke();
                Wait.ForIdle();

                Log.Comment("Get NavView Active VisualStates");
                getNavViewActiveVisualStatesButton.Invoke();
                Wait.ForIdle();

                Verify.IsTrue(result.GetText().Contains(visualStateName), "active VisualStates includes " + visualStateName);
            }
        }

        [TestMethod]
        public void VerifyTopNavigationItemFocusVisualKindRevealTest()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Store Test" }))
            {
                if (!PlatformConfiguration.IsOsVersionGreaterThanOrEqual(OSVersion.Redstone4))
                {
                    Log.Warning("Skipping: Focus Reveal was added starting RS4");
                    return;
                }

                Log.Comment("Getting navitem active visual states");
                var getActiveVisualStateButton = new Button(FindElement.ByName("GetNavItemActiveVisualState"));

                var activeVisualStates = new Edit(FindElement.ById("NavItemActiveVisualStates"));

                // didn't figure out why, retry helps the stability.
                Wait.RetryUntilEvalFuncSuccessOrTimeout(
                    () =>
                    {
                        getActiveVisualStateButton.Click();
                        Wait.ForIdle();
                        return activeVisualStates.GetText().Contains("OnTopNavigationPrimaryReveal");
                    },
                    retryTimoutByMilliseconds: 3000
                );

                Log.Comment("Visual states: " + activeVisualStates.GetText());
                Verify.IsTrue(activeVisualStates.GetText().Contains("OnTopNavigationPrimaryReveal"));
            }
        }

        [TestMethod]
        // To verify two problems:
        // 1. NavigationViewItem not in overflow menu
        //      Layout doesn't know about overflow, so changing the content of NavigationViewItem may not trigger MeasureOverride
        //      Verify NavigationView will handle this
        // 2. NavigationViewItem in overflow menu
        //      We cached the Width when moving items to overflow, and cached width would be used to recover items to primary
        //      This test case verifies that cache is invalidated if content is changed for NavigationViewItem
        public void VerifyNavigationViewContentChangeOnTopNavImpactsLayout()
        {
            using (var setup = new TestSetupHelper(new[] { "NavigationView Tests", "Top NavigationView Test" }))
            {
                Log.Comment("Verify there is overflow button");
                VerifyElement.Found("TopNavOverflowButton", FindBy.Id);

                Verify.IsTrue(IsItemInTopNavPrimaryList("Games"), "Games not in overflow");

                Log.Comment("Change Games Content to a long string");
                Button changeGamesContent = new Button(FindElement.ByName("ChangeGamesContent"));
                changeGamesContent.Invoke();
                Wait.ForIdle();

                Log.Comment("Verify Content change in primary may push Games item to overflow");
                Verify.IsFalse(IsItemInTopNavPrimaryList("Games"), "Games is moved to overflow");

                Log.Comment("Clear Content of all NavigationViewItems");
                Button clearNavItemContent = new Button(FindElement.ByName("ClearNavItemContent"));
                clearNavItemContent.Invoke();
                Wait.ForIdle();

                Log.Comment("Verify all items are moved out of overflow menu");
                VerifyElement.NotFound("TopNavOverflowButton", FindBy.Id);

                Log.Comment("Change Games Content to a long string");
                changeGamesContent.Invoke();
                Wait.ForIdle();

                Log.Comment("Verify change content of Game makes it to overflow");
                Verify.IsFalse(IsItemInTopNavPrimaryList("Games"), "Games is moved to overflow");

                Log.Comment("Clear Content of Games which is in overflow");
                clearNavItemContent.Invoke();
                Wait.ForIdle();

                // If NavigationViewItem is in overflow and the popup is not opened, we can't get NavigationView by the visualtree from NavigationViewItem itself
                // Change the content of NavigationViewItem in overflow will not Invalidate Layout, and request user to invalid measure.
                Log.Comment("Invalid Measure");
                Button invalidateMeasure = new Button(FindElement.ByName("NavInvalidateMeasure"));
                invalidateMeasure.Invoke();
                Wait.ForIdle();

                Log.Comment("Verify cached width is invalidated and InvalidateMeasure will move all items out of overflow");
                VerifyElement.NotFound("TopNavOverflowButton", FindBy.Id);
            }
        }

    }
}
