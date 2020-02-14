#include "daScript/misc/platform.h"

#include "daScript/simulate/runtime_table_nodes.h"

namespace das
{
    void table_clear ( Context & context, Table & arr ) {
        if ( arr.isLocked() ) context.throw_error("can't clear locked table");
        memset(arr.hashes, 0, arr.capacity * sizeof(uint32_t));
        arr.size = 0;
    }

    void table_lock ( Context & context, Table & arr ) {
        if ( arr.shared ) return;
        arr.lock ++;
        if ( arr.lock==0 ) context.throw_error("table lock overflow");
    }

    void table_unlock ( Context & context, Table & arr ) {
        if ( arr.shared ) return;
        if ( arr.lock==0 ) context.throw_error("table lock underflow");
        arr.lock --;
    }

    // TableIterator

    size_t TableIterator::nextValid ( size_t index ) const {
        for (; index < table->capacity; index++) {
            if (table->hashes[index] > HASH_KILLED32) {
                break;
            }
        }
        return index;
    }

    bool TableIterator::first ( Context & context, char * _value ) {
        char ** value = (char **)_value;
        table_lock(context, *(Table *)table);
        size_t index = nextValid(0);
        char * data    = getData();
        *value         = data + index * stride;
        table_end      = data + table->capacity * stride;
        return (bool) table->size;
    }

    bool TableIterator::next  ( Context &, char * _value ) {
        char ** value = (char **) _value;
        char * data = *value;
        char * tableData = getData();
        size_t index = nextValid((data-tableData)/stride+1);
        data = tableData + index * stride;
        *value = data;
        return data != table_end;
    }

    void TableIterator::close ( Context & context, char * _value ) {
        char ** value = (char **) _value;
        *value = nullptr;
        table_unlock(context, *(Table *)table);
    }

    // keys and values

    char * TableKeysIterator::getData ( ) const {
        return table->keys;
    }

    void TableKeysIterator::close ( Context & context, char * value ) {
        TableIterator::close(context,value);
        context.heap.free((char *)this, sizeof(TableKeysIterator));
    }

    char * TableValuesIterator::getData ( ) const {
        return table->data;
    }

    void TableValuesIterator::close ( Context & context, char * value ) {
        TableIterator::close(context,value);
        context.heap.free((char *)this, sizeof(TableValuesIterator));
    }

    void builtin_table_keys ( Sequence & result, const Table & tab, int32_t stride, Context * __context__ ) {
        char * iter = __context__->heap.allocate(sizeof(TableKeysIterator));
        new (iter) TableKeysIterator(&tab, stride);
        result = { (Iterator *) iter };
    }

    void builtin_table_values ( Sequence & result, const Table & tab, int32_t stride, Context * __context__ ) {
        char * iter = __context__->heap.allocate(sizeof(TableKeysIterator));
        new (iter) TableValuesIterator(&tab, stride);
        result = { (Iterator *) iter };
    }

    // delete

    vec4f SimNode_DeleteTable::eval ( Context & context ) {
        DAS_PROFILE_NODE
        auto pTable = (Table *) subexpr->evalPtr(context);
        pTable = pTable + total - 1;
        for ( uint32_t i=0; i!=total; ++i, pTable-- ) {
            if ( pTable->data ) {
                if ( !pTable->isLocked() ) {
                    uint32_t oldSize = pTable->capacity*(vts_add_kts + sizeof(uint32_t));
                    context.heap.free(pTable->data, oldSize);
                } else {
                    context.throw_error("deleting locked table");
                    return v_zero();
                }
            }
            memset ( pTable, 0, sizeof(Table) );
        }
        return v_zero();
    }
}

