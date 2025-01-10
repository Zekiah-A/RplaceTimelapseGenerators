export const decoder: TextDecoder;
export const encoder: TextEncoder;
export class BufReader extends DataView {
	/**  */
	constructor(arr: any);
	i: number;
	bitState: number;
	decode(t: any, v: any): any;
	b1(): number;
	b2(): number;
	b4(): number;
	u8(): number;
	i8(): number;
	u16(): number;
	i16(): number;
	u24(): number;
	i24(): number;
	u32(): number;
	i32(): number;
	u48(): number;
	i48(): number;
	u64(): number;
	i64(): number;
	bu64(): bigint | 0;
	bi64(): bigint | 0;
	f32(): number;
	f64(): number;
	bool(): boolean | 0;
	v64(): number;
	bv64(): bigint | 0;
	v32(): number;
	v16(): number;
	/** Advance a number of bytes. Useful for padding */
	skip(n: any): void;
	u8arr(len?: number): Uint8Array;
	str(): string;
	enum({ intToStr, defaultString }: {
		intToStr: any;
		defaultString: any;
	}): any;
	/** Returns a mutable Uint8Array view of the next bytes */
	view(size?: number): Uint8Array;
	/** How many bytes have been read from this buffer so far */
	get read(): number;
	/** How many more bytes can be read before reaching the end of the buffer */
	get remaining(): number;
	/** Whether we have reached and passed the end of the buffer. All read functions will return "null" values (i.e, 0, "", Uint8Array(0)[], false, ...) */
	get overran(): boolean;
	/** Get a Uint8Array pointing to remaining unread data. This is a reference and not a copy. Use Uint8Array.slice() to turn it into a copy */
	toUint8Array(): Uint8Array;
	/** Copies all the bytes that have already been read since this object's creation into a new ArrayBuffer */
	copyReadToArrayBuffer(): ArrayBuffer;
	/** Copies all the bytes yet to be read into a new ArrayBuffer */
	copyRemainingToArrayBuffer(): ArrayBuffer;
	/** Same as new BufWriter(this.copyReadToArrayBuffer(), this.i) */
	copyToWriter(): BufWriter;
	/** Same as new BufReader(this.copyRemainingToArrayBuffer()) */
	copy(): BufReader;
}
export class BufWriter {
	/** Construct a new BufWriter, optionally passing the underlying ArrayBuffer and head position. Once the head surpasses the ArrayBuffer's length, it is discarded (and possibly detached) and a new ArrayBuffer is allocated and used */
	constructor(arr?: ArrayBuffer, head?: number);
	buf: DataView;
	buf8: Uint8Array;
	i: number;
	cap: number;
	bitState: number;
	_grow(n?: number): void;
	encode(t: any, v: any): any;
	b1(n?: number): void;
	b2(n?: number): void;
	b4(n?: number): void;
	u8(n?: number): void;
	i8(n?: number): void;
	u16(n?: number): void;
	i16(n?: number): void;
	u24(n?: number): void;
	i24(n?: number): void;
	u32(n?: number): void;
	i32(n?: number): void;
	u48(n?: number): void;
	i48(n?: number): void;
	u64(n?: number): void;
	i64(n?: number): void;
	bu64(n?: bigint): void;
	bi64(n?: bigint): void;
	f32(n?: number): void;
	f64(n?: number): void;
	bool(n?: boolean): void;
	v64(n?: number): void;
	bv64(bn?: bigint): void;
	v32(n?: number): void;
	v16(n?: number): void;
	u8arr(v: any, n?: number): void;
	/** Advance a number of bytes. Useful for padding */
	skip(n?: number): void;
	str(v?: string): void;
	enum({ strToInt, default: d }: {
		strToInt: any;
		default: any;
	}, str: any): void;
	/** How many bytes have been written to this buffer so far */
	get written(): number;
	/** The underlying array buffer that is being modified. May be larger than this.written (this is intentional to avoid excessive reallocations). May become detached as writer grows */
	get buffer(): ArrayBuffer;
	get byteOffset(): number;
	get byteLength(): number;
	/** View into the currently written data. May become detached as writer grows, consider using a copying method */
	toUint8Array(): Uint8Array;
	/** Reader for the currently written data. May become detached as writer grows, consider using a copying method */
	toReader(): BufReader;
	/** Get a copy of the written data as an ArrayBuffer */
	copyToArrayBuffer(): ArrayBuffer;
	/** Get a copy of the written data as a second BufWriter */
	copy(): BufWriter;
	/** Same as new BufReader(this.copyToArrayBuffer()) */
	copyToReader(): BufReader;
}
export const b1: any;
export const b2: any;
export const b4: any;
export const u8: any;
export const i8: any;
export const u16: any;
export const i16: any;
export const u24: any;
export const i24: any;
export const u32: any;
export const i32: any;
export const u48: any;
export const i48: any;
export const u64: any;
export const i64: any;
export const bu64: any;
export const bi64: any;
export const f32: any;
export const f64: any;
export const v16: any;
export const v32: any;
export const v64: any;
export const bv64: any;
export const u8arr: any;
export const str: any;
export const bool: any;
export function Struct(obj: any, f: any): any;
export function Arr(type: any, len?: number): any;
export function Optional(t: any): any;
export function Enum(v?: any[], def?: any): any;
export function Padding(sz?: number): any;
