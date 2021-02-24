/*
** Zabbix
** Copyright (C) 2001-2021 Zabbix SIA
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**/

const WIDGET_EVENT_EDIT_CLICK = 'edit-click';
const WIDGET_EVENT_ENTER      = 'enter';
const WIDGET_EVENT_LEAVE      = 'leave';

class CDashboardWidget extends CBaseComponent {

	constructor({
		defaults,
		widgetid = '',
		uniqueid,
		index = 0,
		type = '',
		fields,
		configuration,
		storage = {},
		header = '',
		dynamic_hostid = null,
		view_mode = ZBX_WIDGET_VIEW_MODE_NORMAL,
		rf_rate = 0,
		preloader_timeout = 10000,
		pos = {x: 0, y: 0, width: 1, height: 1},
		cell_width = 0,
		cell_height = 0,
		parent = false,
		update_paused = false,
		initial_load = true,
		is_editable = false,
		is_edit_mode = false,
		is_iterator = false,
		is_new = !widgetid.length,
		is_ready = false,
		css_classes = {
			actions: 'dashbrd-grid-widget-actions',
			container: 'dashbrd-grid-widget-container',
			content: 'dashbrd-grid-widget-content',
			focus: 'dashbrd-grid-widget-focus',
			head: 'dashbrd-grid-widget-head',
			hidden_header: 'dashbrd-grid-widget-hidden-header',
			mask: 'dashbrd-grid-widget-mask',
			root: 'dashbrd-grid-widget'
		}
	} = {}) {
		super(document.createElement('div'));

		this.defaults = defaults;
		this.widgetid = widgetid;
		this.uniqueid = uniqueid;
		this._index = index;
		this.type = type;

		// Replace empty arrays (or anything non-object) with empty objects.
		this.fields = (typeof fields === 'object') ? fields : {};
		this.configuration = (typeof configuration === 'object') ? configuration : {};
		this.storage = storage;

		this.header = header;
		// TODO Remove dynamic_hostid from widget class
		this.dynamic_hostid = (!this.fields.dynamic || this.fields.dynamic != 1) ? null : dynamic_hostid;
		this.view_mode = view_mode;

		this.rf_rate = rf_rate;
		this._preloader_timeout = preloader_timeout;

		this.pos = pos;
		this._cell_width = cell_width;
		this._cell_height = cell_height;

		this.parent = parent;
		this.update_paused = update_paused;
		this.initial_load = initial_load;

		this._is_active = false;
		this._is_iterator = is_iterator;
		this._is_editable = is_editable;
		this._is_edit_mode = is_edit_mode;
		this._is_new = is_new;
		this._is_ready = is_ready;

		this._css_classes = css_classes;

		this._makeView();
	}

	activate() {
		if (!this._is_active) {
			this._is_active = true;

			this._registerEvents();
		}

		return this;
	}

	deactivate() {
		this._is_active = false;

		this._unregisterEvents();

		return this;
	}

	/**
	 * Focus specified top-level widget.
	 */
	enter() {
		this.div.addClass(this._css_classes.focus);
	}

	/**
	 * Focus specified child widget of iterator.
	 */
	enterIteratorWidget() {
		this.div.addClass(this._css_classes.focus);

		if (this.parent.div.hasClass('dashbrd-grid-iterator-hidden-header')) {
			this.parent.div.toggleClass('iterator-double-header', this.div.position().top === 0);
		}
	}

	/**
	 * Blur specified top-level widget.
	 */
	leave() {
		// Widget placeholder doesn't have header.
		if (!this.content_header) {
			return;
		}

		if (this.content_header.has(document.activeElement).length) {
			document.activeElement.blur();
		}

		this.div.removeClass(this._css_classes.focus);
	}

	update({body, messages, info, debug, script_inline}) {
		this.content_body.empty();

		if (messages !== undefined) {
			this.content_body.append(messages);
		}
		this.content_body.append(body);

		if (debug !== undefined) {
			$(debug).appendTo(this.content_body);
		}

		this.removeInfoButtons();
		if (info !== undefined && !this._is_edit_mode) {
			this.addInfoButtons(info);
		}

		// Creates new script elements and removes previous ones to force their re-execution.
		this.content_script.empty();
		if (script_inline !== undefined) {
			// NOTE: to execute script with current widget context, add unique ID for required div, and use it in script.
			this.content_script.append($('<script>').text(script_inline));
		}
	}

	isEditable() {
		return this._is_editable;
	}

	isEditMode() {
		return this._is_edit_mode;
	}

	setEditMode(is_edit_mode) {
		this._is_edit_mode = is_edit_mode;

		return this;
	}

	isReady() {
		return this._is_ready;
	}

	/**
	 * Update widget to ready state.
	 *
	 * @returns {boolean}  Returns true, if status updated.
	 */
	updateReady() {
		let is_ready_updated = false;

		if (this._is_iterator) {
			if (!this.children.length) {
				// Set empty iterator to ready state.

				is_ready_updated = !this._is_ready;
				this._is_ready = true;
			}
		}
		else if (this.parent) {
			this._is_ready = true;

			const children_not_ready = this.parent.children.filter((w) => {
				return !w._is_ready;
			});

			if (!children_not_ready.length) {
				// Set parent iterator to ready state.

				is_ready_updated = !this.parent._is_ready;
				this.parent._is_ready = true;
			}
		}
		else {
			is_ready_updated = !this._is_ready;
			this._is_ready = true;
		}

		return is_ready_updated;
	}

	getCssClass(key) {
		return this._css_classes[key] || '';
	}

	getIndex() {
		return this._index;
	}

	setIndex(index) {
		this._index = index;

		return this;
	}

	setViewMode(view_mode) {
		if (this.view_mode !== view_mode) {
			this.view_mode = view_mode;
			this.div.toggleClass(this._css_classes.hidden_header, view_mode == ZBX_WIDGET_VIEW_MODE_HIDDEN_HEADER);
		}

		return this;
	}

	showPreloader() {
		this.div.find(`.${this._css_classes.content}`).addClass('is-loading');
	}

	hidePreloader() {
		this.div.find(`.${this._css_classes.content}`).removeClass('is-loading');
	}

	startPreloader(timeout = this._preloader_timeout) {
		if (typeof this._preloader_timeoutid !== 'undefined' || this.div.find('.is-loading').length) {
			return;
		}

		this._preloader_timeoutid = setTimeout(() => {
			delete this._preloader_timeoutid;

			this.showPreloader();
		}, timeout);
	}

	stopPreloader() {
		if (typeof this._preloader_timeoutid !== 'undefined') {
			clearTimeout(this._preloader_timeoutid);
			delete this._preloader_timeoutid;
		}

		this.hidePreloader();
	}

	getContentSize() {
		return {
			'content_width': Math.floor(this.content_body.width()),
			'content_height': Math.floor(this.content_body.height())
		};
	}

	clearUpdateContentTimer() {
		if (typeof this.rf_timeoutid !== 'undefined') {
			clearTimeout(this.rf_timeoutid);
			delete this.rf_timeoutid;
		}
	}

	/**
	 * Enable user functional interaction with widget.
	 */
	enableControls() {
		this.content_header.find('button').prop('disabled', false);
	}

	/**
	 * Disable user functional interaction with widget.
	 */
	disableControls() {
		this.content_header.find('button').prop('disabled', true);
	}

	addInfoButtons(buttons) {
		// Note: this function is used only for widgets and not iterators.

		const $widget_actions = $('.dashbrd-grid-widget-actions', this.content_header);

		for (const button of buttons) {
			$widget_actions.prepend(
				$('<li>', {'class': 'widget-info-button'})
					.append(
						$('<button>', {
							'type': 'button',
							'class': button.icon,
							'data-hintbox': 1,
							'data-hintbox-static': 1
						})
					)
					.append(
						$('<div>', {
							'class': 'hint-box',
							'html': button.hint
						}).hide()
					)
			);
		}
	}

	removeInfoButtons() {
		// Note: this function is used only for widgets and not iterators.

		$('.dashbrd-grid-widget-actions', this.content_header).find('.widget-info-button').remove();
	}

	_makeView() {
		this.content_header = $('<div>', {'class': this._css_classes.head})
			.append($('<h4>').text((this.header !== '') ? this.header : this.defaults.header));

		if (!this.parent) {
			const widget_actions = {
				'widgetType': this.type,
				'currentRate': this.rf_rate,
				'widget_uniqueid': this.uniqueid,
				'multiplier': '0'
			};

			// TODO Remove graphid, itemid, dynamic_hostid from widget class
			if ('graphid' in this.fields) {
				widget_actions.graphid = this.fields['graphid'];
			}

			if ('itemid' in this.fields) {
				widget_actions.itemid = this.fields['itemid'];
			}

			if (this.dynamic_hostid !== null) {
				widget_actions.dynamic_hostid = this.dynamic_hostid;
			}

			if (this._is_iterator) {
				this.$button_iterator_previous_page = $('<button>', {
					'type': 'button',
					'class': 'btn-iterator-page-previous',
					'title': t('Previous page')
				});
				this.$button_iterator_next_page = $('<button>', {
					'type': 'button',
					'class': 'btn-iterator-page-next',
					'title': t('Next page')
				});
			}

			if (this._is_editable) {
				this.$button_edit = $('<button>', {
					'type': 'button',
					'class': 'btn-widget-edit',
					'title': t('Edit')
				});
			}

			// Do not add action buttons for child widgets of iterators.
			this.content_header
				.append(this._is_iterator
					? $('<div>', {'class': 'dashbrd-grid-iterator-pager'}).append(
						this.$button_iterator_previous_page,
						$('<span>', {'class': 'dashbrd-grid-iterator-pager-info'}),
						this.$button_iterator_next_page
					)
					: ''
				)

				.append($('<ul>', {'class': this._css_classes.actions})
					.append(this._is_editable
						? $('<li>').append(this.$button_edit)
						: ''
					)
					.append(
						$('<li>').append(
							$('<button>', {
								'type': 'button',
								'class': 'btn-widget-action',
								'title': t('Actions'),
								'data-menu-popup': JSON.stringify({
									'type': 'widget_actions',
									'data': widget_actions
								}),
								'attr': {
									'aria-expanded': false,
									'aria-haspopup': true
								}
							})
						)
					)
				);
		}

		this.content_body = $('<div>', {'class': this._css_classes.content})
			.toggleClass('no-padding', !this._is_iterator && !this.configuration['padding']);

		this.container = $('<div>', {'class': this._css_classes.container})
			.append(this.content_header)
			.append(this.content_body);

		if (this._is_iterator) {
			this.container
				.append($('<div>', {'class': 'dashbrd-grid-iterator-too-small'})
					.append($('<div>').html(t('Widget is too small for the specified number of columns and rows.')))
				);
		}
		else {
			this.content_script = $('<div>');
			this.container.append(this.content_script);
		}

		this.div = $(this._target)
			.addClass(this._css_classes.root)
			.toggleClass(this._css_classes.hidden_header, this.view_mode == ZBX_WIDGET_VIEW_MODE_HIDDEN_HEADER)
			.toggleClass('new-widget', this._is_new);

		if (!this.parent) {
			this.div.css({
				'min-height': `${this._cell_height}px`,
				'min-width': `${this._cell_width}%`
			});
		}

		// Used for disabling widget interactivity in edit mode while resizing.
		this.mask = $('<div>', {'class': this._css_classes.mask});

		this.div.append(this.container, this.mask);
	}

	_registerEvents() {
		this._events = {

			edit: () => {
				this.fire(WIDGET_EVENT_EDIT_CLICK);
			},

			enter: () => {
				this.fire(WIDGET_EVENT_ENTER);
			},

			leave: () => {
				this.fire(WIDGET_EVENT_LEAVE);
			}
		};

		if (!this.parent) {
			if (this._is_editable) {
				this.$button_edit.on('click', this._events.edit);
			}
		}

		this.content_header
			.on('focusin', this._events.enter)
			.on('focusout', (e) => {
				if (!this.content_header.has(e.relatedTarget).length) {
					this._events.leave();
				}
			})
			.on('focusin focusout', () => {
				// Skip mouse events caused by animations which were caused by focus change.
				this._mousemove_waiting = true;
			});

		this.div
			// "Mouseenter" is required, since "mousemove" may not always bubble.
			.on('mouseenter mousemove', () => {
				this._events.enter();

				delete this._mousemove_waiting;
			})
			.on('mouseleave', () => {
				if (!this._mousemove_waiting) {
					this._events.leave();
				}
			})
			.on('load.image', () => {
				// Call refreshCallback handler for expanded popup menu items.
				const $menu_popup = this.div.find('[data-expanded="true"][data-menu-popup]');

				if ($menu_popup.length) {
					$menu_popup.menuPopup('refresh', this);
				}
			});
	}

	_unregisterEvents() {
		if (!this.parent) {
			if (this._is_editable) {
				this.$button_edit.off('click', this._events.edit);
			}
		}

		this.content_header.off('focusin focusout');

		this.div.off('mouseenter mousemove mouseleave load.image');
	}
}