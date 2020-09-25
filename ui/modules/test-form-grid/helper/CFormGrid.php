<?php

declare(strict_types=1);

namespace Modules\FormGrid\Helper;

use CTag;

class CFormGrid extends CTag
{

	public const ZBX_STYLE_FORM_GRID = 'form-grid';

	public function __construct()
	{
		parent::__construct('div', true);

		parent::addClass(self::ZBX_STYLE_FORM_GRID);
	}

	public function toString($destroy = true)
	{
		return parent::toString($destroy);
	}
}

abstract class CFormGridItem extends CTag
{

	public const ZBX_STYLE_FORM_GRID_ITEM = 'form-grid-item';
	public const ZBX_STYLE_FORM_GRID_ITEM_LABEL = 'col-label';
	public const ZBX_STYLE_FORM_GRID_ITEM_DETAIL = 'col-detail';
	public const ZBX_STYLE_FORM_GRID_ITEM_LABEL_SMALL = 'col-sm-label';
	public const ZBX_STYLE_FORM_GRID_ITEM_DETAIL_SMALL = 'col-sm-detail';
	public const ZBX_STYLE_FORM_GRID_ITEM_OFFSET_1 = 'col-offset-1';
	public const ZBX_STYLE_FORM_GRID_ITEM_OFFSET_2 = 'col-offset-2';
	public const ZBX_STYLE_FORM_GRID_ITEM_OFFSET_3 = 'col-offset-3';

	public const OFFSET_1 = 1;
	public const OFFSET_2 = 2;
	public const OFFSET_3 = 3;

	protected $is_small = false;

	public function __construct($value)
	{
		parent::__construct('div', true);
		parent::addClass(self::ZBX_STYLE_FORM_GRID_ITEM);

		$this->addItem($value);
	}

	public function setOffset(int $flag): self
	{
		$offsets = [
			self::OFFSET_1 => self::ZBX_STYLE_FORM_GRID_ITEM_OFFSET_1,
			self::OFFSET_2 => self::ZBX_STYLE_FORM_GRID_ITEM_OFFSET_2,
			self::OFFSET_3 => self::ZBX_STYLE_FORM_GRID_ITEM_OFFSET_3
		];

		if (!array_key_exists($flag, $offsets)) {
			throw new \Exception('Unknown offset');
		}

		parent::addClass($offsets[$flag]);

		return $this;
	}

	public function setSmall(): self
	{
		$this->is_small = true;

		return $this;
	}
}

class CFormGridItemLabel extends CFormGridItem
{
	public function toString($destroy = true)
	{
		parent::addClass($this->is_small ? self::ZBX_STYLE_FORM_GRID_ITEM_LABEL_SMALL : self::ZBX_STYLE_FORM_GRID_ITEM_LABEL);

		return parent::toString($destroy);
	}
}

class CFormGridItemDetail extends CFormGridItem
{
	public function toString($destroy = true)
	{
		parent::addClass($this->is_small ? self::ZBX_STYLE_FORM_GRID_ITEM_DETAIL_SMALL : self::ZBX_STYLE_FORM_GRID_ITEM_DETAIL);

		return parent::toString($destroy);
	}
}
